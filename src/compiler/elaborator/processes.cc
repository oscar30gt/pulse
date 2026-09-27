#include "elaborator_internal.h"

#include <algorithm>
#include <limits>

namespace Pulse::Parser
{
    using namespace Engine;

    namespace
    {
        /// Loops are unrolled; beyond this many iterations the design is refused rather than blown up.
        constexpr int64_t kMaxUnrolledIterations = 4096;

        /// The signal a target assigns in part (an element or a slice), also inside an aggregate target.
        void collectPartialTarget(const Expression& target, const DesignLibrary& library, std::unordered_set<const ASTNode*>& partial)
        {
            if (auto* aggregate = dynamic_cast<const AggregateExpr*>(&target))
            {
                for (const auto& element : aggregate->elements)
                    collectPartialTarget(*element, library, partial);
                return;
            }
            if (auto* call = dynamic_cast<const FunctionCallExpr*>(&target))
                if (auto* name = dynamic_cast<const SymbolExpr*>(call->callee.get()))
                    if (const ASTNode* declaration = library.declarationOf(*name))
                        partial.insert(declaration);
        }

        /// The signals a process assigns in parts somewhere in `statements` (they need a shadow wire).
        void collectPartialTargets(const std::vector<StatementPtr>& statements, const DesignLibrary& library,
                                   std::unordered_set<const ASTNode*>& partial)
        {
            for (const auto& statement : statements)
            {
                if (auto* assignment = dynamic_cast<const SignalAssignment*>(statement.get()))
                    collectPartialTarget(*assignment->target, library, partial);
                else if (auto* with = dynamic_cast<const WithClause*>(statement.get()))
                    collectPartialTarget(*with->target, library, partial);
                else if (auto* branches = dynamic_cast<const IfStatement*>(statement.get()))
                {
                    for (const auto& branch : branches->branches)
                        collectPartialTargets(branch->body, library, partial);
                    collectPartialTargets(branches->elseBody, library, partial);
                }
                else if (auto* choice = dynamic_cast<const CaseStatement*>(statement.get()))
                {
                    for (const auto& alternative : choice->alternatives)
                        collectPartialTargets(alternative->body, library, partial);
                }
                else if (auto* loop = dynamic_cast<const ForLoopStatement*>(statement.get()))
                    collectPartialTargets(loop->body, library, partial);
            }
        }

        std::string processDescription(const ProcessContext& process)
        {
            return process.name.front() == '$' ? "the process at line " + std::to_string(process.location.line)
                                               : "process '" + process.name + "'";
        }
    } // anonymous namespace

    // ---- Processes ------------------------------------------------------------------------------

    void UnitElaborator::elaborateProcess(const ProcessStatement& process)
    {
        ProcessContext context;
        context.name = process.label.empty() ? "$process#" + std::to_string(m_nextProcess++) : process.label;
        context.statement = &process;
        context.location = process.source;
        collectPartialTargets(process.body, m_library, context.partialTargets);

        m_process = &context;
        m_typeScopes.emplace_back();

        elaborateDeclarations(process.declarations, true);
        lowerSequence(process.body);

        const bool combinational = process.sensitivityAll || !process.sensitivityList.empty();
        std::vector<std::string> sensitivity = process.sensitivityAll ? context.signalsRead : sensitivityOf(process.sensitivityList);

        m_typeScopes.pop_back();
        m_process = nullptr;

        std::vector<std::string> inPorts;
        for (const std::string& wire : context.builder.reads())
            if (std::find(context.builder.writes().begin(), context.builder.writes().end(), wire) == context.builder.writes().end())
                inPorts.push_back(wire);
        std::vector<std::string> outPorts = context.builder.writes();

        auto instance = std::make_unique<ProcessInstance>(std::move(inPorts), std::move(outPorts), ProcessProgram(context.builder.finish()),
                                                          std::move(sensitivity));
        instance->combinational = combinational;
        m_bp.addComponent(context.name, std::move(instance));
    }

    void UnitElaborator::elaborateEquivalentProcess(const SignalAssignment& assignment)
    {
        // LRM 11.6: the process a concurrent signal assignment stands for, sensitive to every signal it reads.
        ProcessContext context;
        context.name = assignment.label.empty() ? "$process#" + std::to_string(m_nextProcess++) : assignment.label;
        context.statement = nullptr;
        context.location = assignment.source;
        collectPartialTarget(*assignment.target, m_library, context.partialTargets);
        m_process = &context;

        lowerSequentialAssignment(*assignment.target, *assignment.value, true, assignment);

        m_process = nullptr;

        std::vector<std::string> inPorts;
        for (const std::string& wire : context.builder.reads())
            if (std::find(context.builder.writes().begin(), context.builder.writes().end(), wire) == context.builder.writes().end())
                inPorts.push_back(wire);

        auto instance = std::make_unique<ProcessInstance>(std::move(inPorts), context.builder.writes(), ProcessProgram(context.builder.finish()),
                                                          context.signalsRead);
        instance->combinational = true;
        m_bp.addComponent(context.name, std::move(instance));
    }

    std::vector<std::string> UnitElaborator::sensitivityOf(const std::vector<ExpressionPtr>& names)
    {
        std::vector<std::string> wires;
        for (const auto& name : names)
        {
            // An element or a slice in a list stands for its whole signal here.
            const Expression* root = name.get();
            while (auto* call = dynamic_cast<const FunctionCallExpr*>(root))
                root = call->callee.get();

            const ObjectWire* object = objectOf(*root);
            if (!object)
                unsupported("This name in a sensitivity list", *name);
            if (std::find(wires.begin(), wires.end(), object->wire) == wires.end())
                wires.push_back(object->wire);
        }
        return wires;
    }

    // ---- Objects of a process -------------------------------------------------------------------

    std::string UnitElaborator::variableWire(const VariableDeclaration& decl)
    {
        const SemanticType* type = m_library.objectType(decl);
        if (!type)
            fail("Variable '" + decl.name + "' was not analyzed", decl);

        const Layout layout = layoutOf(*type, decl.typeSpec.get(), decl);
        const LogicVector initial = decl.initialValue
            ? bitsOf(requireStatic(*decl.initialValue, "The initial value of variable '" + decl.name + "'", &layout), layout, *decl.initialValue)
            : defaultValue(layout, *type);

        const std::string wire = newWire(layout.width, m_process->name + "." + decl.name, initial);

        ObjectWire object;
        object.objectClass = ObjectWire::Class::Variable;
        object.name = decl.name;
        object.wire = wire;
        object.layout = layout;
        object.declaration = &decl;
        object.defaultValue = initial;
        m_objects[&decl] = object;
        return wire;
    }

    std::string UnitElaborator::shadowOf(const ObjectWire& signal)
    {
        auto found = m_process->shadows.find(signal.declaration);
        if (found != m_process->shadows.end())
            return found->second;

        // The value this process will give the signal. The process is the only driver of the signal, so the shadow and
        // the signal hold the same value whenever the process resumes.
        const std::string wire = newWire(signal.layout.width, m_process->name + "." + signal.name + ".next", signal.defaultValue);
        m_process->shadows.emplace(signal.declaration, wire);
        return wire;
    }

    // ---- Sequential statements ------------------------------------------------------------------

    void UnitElaborator::lowerSequence(const std::vector<StatementPtr>& statements)
    {
        for (const auto& statement : statements)
            lowerSequential(*statement);
    }

    void UnitElaborator::lowerSequential(const Statement& statement)
    {
        if (auto* n = dynamic_cast<const SignalAssignment*>(&statement))
            lowerSequentialAssignment(*n->target, *n->value, true, *n);
        else if (auto* n = dynamic_cast<const VariableAssignment*>(&statement))
            lowerSequentialAssignment(*n->target, *n->value, false, *n);
        else if (auto* n = dynamic_cast<const IfStatement*>(&statement))
            lowerIf(*n);
        else if (auto* n = dynamic_cast<const CaseStatement*>(&statement))
            lowerCase(*n);
        else if (auto* n = dynamic_cast<const WithClause*>(&statement))
            lowerSelected(*n);
        else if (auto* n = dynamic_cast<const ForLoopStatement*>(&statement))
            lowerForLoop(*n);
        else if (auto* n = dynamic_cast<const ExitStatement*>(&statement))
            lowerExitOrNext(n->loopLabel, n->condition.get(), true, *n);
        else if (auto* n = dynamic_cast<const NextStatement*>(&statement))
            lowerExitOrNext(n->loopLabel, n->condition.get(), false, *n);
        else if (auto* n = dynamic_cast<const WaitStatement*>(&statement))
            lowerWait(*n);
        else if (dynamic_cast<const NullStatement*>(&statement))
            return;
        else if (dynamic_cast<const WhileLoopStatement*>(&statement) || dynamic_cast<const LoopStatement*>(&statement))
            unsupported("A 'while' loop or a loop without a 'for' range (only 'for' loops with a static range can be unrolled)", statement);
        else if (dynamic_cast<const AssertStatement*>(&statement))
            unsupported("The assert statement", statement);
        else if (dynamic_cast<const ReportStatement*>(&statement))
            unsupported("The report statement", statement);
        else if (dynamic_cast<const ProcedureCallStatement*>(&statement))
            unsupported("Calling a procedure", statement);
        else
            unsupported("This sequential statement", statement);
    }

    void UnitElaborator::lowerSequentialAssignment(const Expression& target, const Expression& value, bool signal, const ASTNode& at)
    {
        if (dynamic_cast<const UnaffectedExpr*>(&value))
            return;

        // `t <= a when c else b ...` in a process is an if statement over plain assignments.
        if (auto* conditional = dynamic_cast<const WhenElseExpr*>(&value))
        {
            InstructionBuilder& builder = m_process->builder;
            const InstructionBuilder::Label end = builder.newLabel();
            const Expression* current = conditional;

            while (auto* branch = dynamic_cast<const WhenElseExpr*>(current))
            {
                const InstructionBuilder::Label next = builder.newLabel();
                builder.branchUnless(lowerCondition(*branch->condition), next);
                lowerSequentialAssignment(target, *branch->trueValue, signal, at);
                builder.jump(end);
                builder.place(next);
                current = branch->falseValue.get();
            }
            if (current)
                lowerSequentialAssignment(target, *current, signal, at);
            builder.place(end);
            return;
        }

        if (auto* aggregate = dynamic_cast<const AggregateExpr*>(&target))
        {
            std::vector<TargetPart> parts;
            bitWidth_t width = 0;
            for (const auto& element : aggregate->elements)
            {
                if (dynamic_cast<const NamedAssociationExpr*>(element.get()))
                    unsupported("Named associations in an aggregate target", *element);
                parts.push_back(targetPart(*element));
                width = static_cast<bitWidth_t>(width + parts.back().high - parts.back().low + 1);
            }

            const Layout layout = layoutOfExpression(value, width);
            const std::string whole = lowerTo(value, layout);
            bitWidth_t next = width;
            for (size_t i = 0; i < parts.size(); ++i)
            {
                const auto size = static_cast<bitWidth_t>(parts[i].high - parts[i].low + 1);
                next = static_cast<bitWidth_t>(next - size);
                assignTarget(*aggregate->elements[i], slice(whole, static_cast<bitWidth_t>(next + size - 1), next), signal, at);
            }
            return;
        }

        const Layout layout = targetPart(target).layout;
        assignTarget(target, lowerTo(value, layout), signal, at);
    }

    void UnitElaborator::assignTarget(const Expression& target, const std::string& wire, bool signal, const ASTNode& at)
    {
        const TargetPart part = targetPart(target);
        const ObjectWire& object = *part.object;
        InstructionBuilder& builder = m_process->builder;

        /// The whole value after writing `wire` into the part of `base` the target designates.
        const auto splice = [&](const std::string& base)
        {
            if (part.whole)
                return wire;

            const bitWidth_t width = object.layout.width;
            std::string value = wire;
            bitWidth_t built = static_cast<bitWidth_t>(part.high - part.low + 1);
            if (part.low > 0)
            {
                value = concat(value, slice(base, static_cast<bitWidth_t>(part.low - 1), 0), static_cast<bitWidth_t>(built + part.low));
                built = static_cast<bitWidth_t>(built + part.low);
            }
            if (part.high < width - 1)
                value = concat(slice(base, static_cast<bitWidth_t>(width - 1), static_cast<bitWidth_t>(part.high + 1)), value, width);
            return value;
        };

        if (!signal)
        {
            if (object.objectClass != ObjectWire::Class::Variable)
                fail("Only variables are assigned with ':='", target);
            builder.assign(object.wire, splice(object.wire), false);
            return;
        }

        if (object.objectClass == ObjectWire::Class::Variable)
            fail("A variable is assigned with ':='", target);

        recordDriver(object, processDescription(*m_process), true, at);

        if (!m_process->partialTargets.count(object.declaration))
        {
            builder.assign(object.wire, wire, true);
            return;
        }

        // A signal assigned in parts: the parts go into its shadow at once, and the shadow into the signal when every
        // process has run. So `v(0) <= a; v(1) <= b;` sets both elements.
        const std::string shadow = shadowOf(object);
        builder.assign(shadow, splice(shadow), false);
        builder.assign(object.wire, shadow, true);
    }

    void UnitElaborator::lowerIf(const IfStatement& statement)
    {
        InstructionBuilder& builder = m_process->builder;
        const InstructionBuilder::Label end = builder.newLabel();

        for (const auto& branch : statement.branches)
        {
            const InstructionBuilder::Label next = builder.newLabel();
            builder.branchUnless(lowerCondition(*branch->condition), next);
            lowerSequence(branch->body);
            builder.jump(end);
            builder.place(next);
        }
        lowerSequence(statement.elseBody);
        builder.place(end);
    }

    void UnitElaborator::lowerCase(const CaseStatement& statement)
    {
        InstructionBuilder& builder = m_process->builder;
        const InstructionBuilder::Label end = builder.newLabel();
        const Operand selector = lower(*statement.selector);

        for (const auto& alternative : statement.alternatives)
        {
            auto* choices = dynamic_cast<const ChoiceListExpr*>(alternative->choices.get());
            if (!choices)
                fail("A case alternative chooses with a list of choices", *alternative);

            if (choices->alternatives.size() == 1 && dynamic_cast<const OthersExpr*>(choices->alternatives.front().get()))
            {
                lowerSequence(alternative->body);
                continue;
            }

            const InstructionBuilder::Label next = builder.newLabel();
            builder.branchUnless(lowerChoiceMatch(selector, *choices, statement.matching), next);
            lowerSequence(alternative->body);
            builder.jump(end);
            builder.place(next);
        }
        builder.place(end);
    }

    void UnitElaborator::lowerSelected(const WithClause& with)
    {
        InstructionBuilder& builder = m_process->builder;
        const InstructionBuilder::Label end = builder.newLabel();
        const Operand selector = lower(*with.selector);

        for (const auto& choice : with.choices)
        {
            auto* choices = dynamic_cast<const ChoiceListExpr*>(choice->choices.get());
            if (!choices)
                fail("A selected assignment chooses with a list of choices", *choice);

            if (choices->alternatives.size() == 1 && dynamic_cast<const OthersExpr*>(choices->alternatives.front().get()))
            {
                lowerSequentialAssignment(*with.target, *choice->value, true, with);
                continue;
            }

            const InstructionBuilder::Label next = builder.newLabel();
            builder.branchUnless(lowerChoiceMatch(selector, *choices, with.matching), next);
            lowerSequentialAssignment(*with.target, *choice->value, true, with);
            builder.jump(end);
            builder.place(next);
        }
        builder.place(end);
    }

    void UnitElaborator::lowerForLoop(const ForLoopStatement& loop)
    {
        const StaticRange range = evaluateRange(*loop.range, "The range of a 'for' loop");
        if (range.length() > kMaxUnrolledIterations)
            unsupported("A 'for' loop of more than " + std::to_string(kMaxUnrolledIterations) + " iterations", loop);

        InstructionBuilder& builder = m_process->builder;
        m_process->loops.push_back({ loop.label, builder.newLabel(), 0 });

        for (int64_t i = 0; i < range.length(); ++i)
        {
            m_values[&loop] = range.enumeration ? StaticValue::ofEnumeration(range.at(i), range.type)
                                                : StaticValue::ofInteger(range.at(i), m_library.predefinedType("integer"));
            m_process->loops.back().nextLabel = builder.newLabel();
            lowerSequence(loop.body);
            builder.place(m_process->loops.back().nextLabel);
        }

        builder.place(m_process->loops.back().exitLabel);
        m_process->loops.pop_back();
        m_values.erase(&loop);
    }

    void UnitElaborator::lowerExitOrNext(const std::string& label, const Expression* condition, bool exit, const ASTNode& at)
    {
        auto& loops = m_process->loops;
        auto frame = loops.rbegin();
        if (!label.empty())
            frame = std::find_if(loops.rbegin(), loops.rend(), [&](const LoopFrame& loop) { return loop.label == label; });
        if (frame == loops.rend())
            fail(std::string(exit ? "'exit'" : "'next'") + " outside of a 'for' loop", at);

        InstructionBuilder& builder = m_process->builder;
        const InstructionBuilder::Label target = exit ? frame->exitLabel : frame->nextLabel;
        if (!condition)
        {
            builder.jump(target);
            return;
        }

        const InstructionBuilder::Label skip = builder.newLabel();
        builder.branchUnless(lowerCondition(*condition), skip);
        builder.jump(target);
        builder.place(skip);
    }

    void UnitElaborator::lowerWait(const WaitStatement& statement)
    {
        InstructionBuilder& builder = m_process->builder;

        std::optional<simTime_t> timeout;
        if (statement.timeout)
        {
            const StaticValue value = requireStatic(*statement.timeout, "The time of a 'wait for'");
            if (value.kind != StaticValue::Kind::Physical || value.integer < 0)
                fail("A 'wait for' needs a non-negative time", *statement.timeout);
            // Each simulation tick is one femtosecond, the base unit of time; a zero wait still lasts one tick (a delta).
            timeout = std::max<simTime_t>(static_cast<simTime_t>(value.integer), 1);
        }

        if (statement.onSignals.empty() && !statement.until)
        {
            if (timeout)
                builder.wait(*timeout);
            else
                builder.waitForever();
            return;
        }

        // `wait until c` without `on` waits on every signal the condition reads (LRM 10.2).
        std::vector<std::string> sensitivity = sensitivityOf(statement.onSignals);
        std::string condition;
        if (statement.until)
        {
            std::vector<std::string> read;
            std::vector<std::string>* outer = m_process->readSink;
            m_process->readSink = &read;
            condition = lowerCondition(*statement.until);
            m_process->readSink = outer;
            if (statement.onSignals.empty())
                sensitivity = read;
        }
        builder.waitOn(sensitivity, condition, timeout);
    }

} // namespace Pulse::Parser

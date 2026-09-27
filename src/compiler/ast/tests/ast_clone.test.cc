// ast_clone.test.cc — every AST node can be cloned, deeply and with its dynamic type preserved.
//
// A populated instance of every concrete node type is built (all children set and distinct), so a
// node that forgets a member in its clone() shows up as a printed difference, and a node type without a
// clone() cannot be instantiated at all. A second table default-constructs every node to check that
// null children stay null.

#include <gtest/gtest.h>

#include <functional>
#include <iostream>
#include <sstream>
#include <set>
#include <string>
#include <typeindex>
#include <vector>

#include "ast.h"

using namespace Pulse::Parser;

namespace
{
    // ---- Builders -------------------------------------------------------------

    ExpressionPtr sym(const std::string& name)
    {
        auto e = std::make_unique<SymbolExpr>();
        e->name = name;
        return e;
    }

    ExpressionPtr num(int64_t value)
    {
        auto e = std::make_unique<IntegerLiteralExpr>();
        e->value = value;
        return e;
    }

    ExpressionPtr text(const std::string& value)
    {
        auto e = std::make_unique<StringLiteralExpr>();
        e->value = value;
        return e;
    }

    ExpressionPtr ch(const std::string& quoted)
    {
        auto e = std::make_unique<CharacterLiteralExpr>();
        e->value = quoted;
        return e;
    }

    ExpressionPtr binary(BinaryOperator op, ExpressionPtr left, ExpressionPtr right)
    {
        auto e = std::make_unique<BinaryOpExpr>();
        e->op = op;
        e->left = std::move(left);
        e->right = std::move(right);
        return e;
    }

    ExpressionPtr range(int64_t high, int64_t low)
    {
        return binary(BinaryOperator::Downto, num(high), num(low));
    }

    std::unique_ptr<TypeSpec> type(const std::string& name, ExpressionPtr constraint = nullptr)
    {
        auto t = std::make_unique<TypeSpec>();
        t->typeName = name;
        if (constraint)
            t->args.push_back(std::move(constraint));
        return t;
    }

    ExpressionPtr choices(ExpressionPtr first)
    {
        auto list = std::make_unique<ChoiceListExpr>();
        list->alternatives.push_back(std::move(first));
        list->alternatives.push_back(num(9));
        return list;
    }

    StatementPtr assignment(const std::string& target, ExpressionPtr value)
    {
        auto s = std::make_unique<SignalAssignment>();
        s->target = sym(target);
        s->value = std::move(value);
        return s;
    }

    std::vector<StatementPtr> body()
    {
        std::vector<StatementPtr> list;
        list.push_back(assignment("q", sym("d")));
        list.push_back(std::make_unique<NullStatement>());
        return list;
    }

    std::unique_ptr<SubprogramSpec> spec()
    {
        auto s = std::make_unique<SubprogramSpec>();
        s->kind = SubprogramKind::Function;
        s->impure = true;
        s->name = "f";
        auto p = std::make_unique<ParameterDeclaration>();
        p->name = "a";
        p->typeSpec = type("bit");
        s->parameters.push_back(std::move(p));
        s->returnType = type("bit");
        return s;
    }

    template <typename T>
    std::unique_ptr<T> with(std::unique_ptr<T> node, const std::function<void(T&)>& fill)
    {
        fill(*node);
        return node;
    }

    template <typename T>
    std::unique_ptr<ASTNode> make(const std::function<void(T&)>& fill)
    {
        return with(std::make_unique<T>(), fill);
    }

    struct Factory
    {
        std::string name;
        std::function<std::unique_ptr<ASTNode>()> create;
    };

    /// One fully populated instance of every concrete node type.
    std::vector<Factory> populatedNodes()
    {
        std::vector<Factory> nodes;
        auto add = [&](const char* name, std::function<std::unique_ptr<ASTNode>()> create)
        {
            nodes.push_back({ name, std::move(create) });
        };

        // Expressions: names
        add("SymbolExpr",           [] { return make<SymbolExpr>([](auto& n) { n.name = "clk"; }); });
        add("FieldAccessExpr",      [] { return make<FieldAccessExpr>([](auto& n) { n.target = sym("p"); n.fieldName = "x"; }); });
        add("FunctionCallExpr",     [] { return make<FunctionCallExpr>([](auto& n) { n.callee = sym("f"); n.arguments.push_back(num(1)); n.arguments.push_back(range(3, 0)); }); });
        add("SignatureExpr",        [] { return make<SignatureExpr>([](auto& n) { n.parameters.push_back(sym("bit")); n.returnType = sym("integer"); }); });
        add("AttributeExpr",        [] { return make<AttributeExpr>([](auto& n) { n.prefix = sym("f"); n.attributeName = "path_name"; n.signature = std::make_unique<SignatureExpr>(); n.signature->parameters.push_back(sym("bit")); }); });
        add("QualifiedExpr",        [] { return make<QualifiedExpr>([](auto& n) { n.typeMark = type("unsigned", range(3, 0)); n.operand = sym("v"); }); });
        add("NamedAssociationExpr", [] { return make<NamedAssociationExpr>([](auto& n) { n.formal = sym("a"); n.actual = sym("b"); }); });
        add("OpenExpr",             [] { return make<OpenExpr>([](auto&) {}); });
        add("ExternalNameExpr",     [] { return make<ExternalNameExpr>([](auto& n) { n.objectClass = ExternalObjectClass::Variable; n.path = ".tb.dut.count"; n.subtype = type("unsigned", range(3, 0)); }); });
        add("TypeSpec",             [] { return make<TypeSpec>([](auto& n) { n.resolution = sym("resolved"); n.typeName = "integer"; n.args.push_back(sym("open")); n.range = range(9, 0); }); });
        add("ElementResolutionExpr",[] { return make<ElementResolutionExpr>([](auto& n) { n.resolution = sym("resolved"); }); });

        // Expressions: literals
        add("IntegerLiteralExpr",   [] { return make<IntegerLiteralExpr>([](auto& n) { n.value = 42; }); });
        add("DoubleLiteralExpr",    [] { return make<DoubleLiteralExpr>([](auto& n) { n.value = 2.5; }); });
        add("PhysicalLiteralExpr",  [] { return make<PhysicalLiteralExpr>([](auto& n) { n.magnitude = num(10); n.unit = "ns"; }); });
        add("CharacterLiteralExpr", [] { return make<CharacterLiteralExpr>([](auto& n) { n.value = "'Z'"; }); });
        add("StringLiteralExpr",    [] { return make<StringLiteralExpr>([](auto& n) { n.value = "hello"; }); });

        // Expressions: choices, aggregates, operators
        add("OthersExpr",           [] { return make<OthersExpr>([](auto&) {}); });
        add("AllExpr",              [] { return make<AllExpr>([](auto&) {}); });
        add("ChoiceListExpr",       [] { return choices(num(1)); });
        add("AggregateExpr",        [] { return make<AggregateExpr>([](auto& n) {
                                            n.elements.push_back(ch("'1'"));
                                            auto named = std::make_unique<NamedAssociationExpr>();
                                            named->formal = choices(num(2));
                                            named->actual = ch("'0'");
                                            n.elements.push_back(std::move(named)); }); });
        add("BinaryOpExpr",         [] { return binary(BinaryOperator::Add, sym("a"), num(1)); });
        add("UnaryOpExpr",          [] { return make<UnaryOpExpr>([](auto& n) { n.op = UnaryOperator::Not; n.operand = sym("a"); }); });
        add("WhenElseExpr",         [] { return make<WhenElseExpr>([](auto& n) { n.trueValue = sym("a"); n.condition = sym("c"); n.falseValue = sym("b"); }); });
        add("UnaffectedExpr",       [] { return make<UnaffectedExpr>([](auto&) {}); });

        // Statements
        add("SignalAssignment",     [] { return assignment("q", sym("d")); });
        add("SelectedChoice",       [] { return make<SelectedChoice>([](auto& n) { n.value = sym("a"); n.choices = choices(num(0)); }); });
        add("WithClause",           [] { return make<WithClause>([](auto& n) {
                                            n.matching = true; n.selector = sym("sel"); n.target = sym("y");
                                            auto c = std::make_unique<SelectedChoice>(); c->value = sym("a"); c->choices = choices(num(0));
                                            n.choices.push_back(std::move(c)); }); });
        add("ComponentInstantiation",[] { return make<ComponentInstantiation>([](auto& n) {
                                            n.componentName = "adder"; n.genericMap.push_back(num(8));
                                            auto a = std::make_unique<NamedAssociationExpr>(); a->formal = sym("x"); a->actual = sym("y");
                                            n.portMap.push_back(std::move(a)); n.portMap.push_back(sym("z")); }); });
        add("ProcessStatement",     [] { return make<ProcessStatement>([](auto& n) {
                                            n.sensitivityAll = true; n.sensitivityList.push_back(sym("clk"));
                                            auto v = std::make_unique<VariableDeclaration>(); v->name = "tmp"; v->typeSpec = type("integer");
                                            n.declarations.push_back(std::move(v)); n.body = body(); }); });
        add("VariableAssignment",   [] { return make<VariableAssignment>([](auto& n) { n.target = sym("v"); n.value = num(1); }); });
        add("IfBranch",             [] { return make<IfBranch>([](auto& n) { n.condition = sym("c"); n.body = body(); }); });
        add("IfStatement",          [] { return make<IfStatement>([](auto& n) {
                                            for (int i = 0; i < 2; ++i) { auto b = std::make_unique<IfBranch>(); b->condition = sym("c" + std::to_string(i)); b->body = body(); n.branches.push_back(std::move(b)); }
                                            n.elseBody = body(); }); });
        add("CaseAlternative",      [] { return make<CaseAlternative>([](auto& n) { n.choices = choices(num(1)); n.body = body(); }); });
        add("CaseStatement",        [] { return make<CaseStatement>([](auto& n) {
                                            n.matching = true; n.selector = sym("s");
                                            auto a = std::make_unique<CaseAlternative>(); a->choices = choices(num(1)); a->body = body();
                                            n.alternatives.push_back(std::move(a)); }); });
        add("ForLoopStatement",     [] { return make<ForLoopStatement>([](auto& n) { n.parameter = "i"; n.range = range(7, 0); n.body = body(); }); });
        add("WhileLoopStatement",   [] { return make<WhileLoopStatement>([](auto& n) { n.condition = sym("c"); n.body = body(); }); });
        add("LoopStatement",        [] { return make<LoopStatement>([](auto& n) { n.body = body(); }); });
        add("ExitStatement",        [] { return make<ExitStatement>([](auto& n) { n.loopLabel = "outer"; n.condition = sym("c"); }); });
        add("NextStatement",        [] { return make<NextStatement>([](auto& n) { n.loopLabel = "outer"; n.condition = sym("c"); }); });
        add("NullStatement",        [] { return make<NullStatement>([](auto&) {}); });
        add("WaitStatement",        [] { return make<WaitStatement>([](auto& n) {
                                            n.onSignals.push_back(sym("clk")); n.until = sym("c");
                                            auto timeout = std::make_unique<PhysicalLiteralExpr>(); timeout->magnitude = num(5); timeout->unit = "ns";
                                            n.timeout = std::move(timeout); }); });
        add("AssertStatement",      [] { return make<AssertStatement>([](auto& n) { n.condition = sym("c"); n.message = text("boom"); n.severity = sym("error"); }); });
        add("ReportStatement",      [] { return make<ReportStatement>([](auto& n) { n.message = text("boom"); n.severity = sym("note"); }); });
        add("ReturnStatement",      [] { return make<ReturnStatement>([](auto& n) { n.value = num(1); }); });
        add("ProcedureCallStatement",[] { return make<ProcedureCallStatement>([](auto& n) {
                                            auto c = std::make_unique<FunctionCallExpr>(); c->callee = sym("p"); c->arguments.push_back(num(1));
                                            n.call = std::move(c); }); });

        // Type definitions
        add("NumericTypeDefinition",[] { return make<NumericTypeDefinition>([](auto& n) { n.range = binary(BinaryOperator::To, num(0), num(255)); }); });
        add("EnumeratedTypeDefinition",[] { return make<EnumeratedTypeDefinition>([](auto& n) { n.literals = { "idle", "'0'", "run" }; }); });
        add("UnitDeclaration",      [] { return make<UnitDeclaration>([](auto& n) { n.name = "ps"; n.multiplier = num(1000); n.ofUnit = "fs"; }); });
        add("PhysicalTypeDefinition",[] { return make<PhysicalTypeDefinition>([](auto& n) {
                                            n.range = binary(BinaryOperator::To, num(0), num(100));
                                            auto u = std::make_unique<UnitDeclaration>(); u->name = "fs"; n.units.push_back(std::move(u)); }); });
        add("ArrayTypeDefinition",  [] { return make<ArrayTypeDefinition>([](auto& n) { n.indexRanges.push_back(range(7, 0)); n.elementType = type("bit"); }); });
        add("UnconstrainedArrayTypeDefinition",[] { return make<UnconstrainedArrayTypeDefinition>([](auto& n) { n.indexTypeMarks = { "natural" }; n.elementType = type("bit"); }); });
        add("RecordField",          [] { return make<RecordField>([](auto& n) { n.name = "x"; n.type = type("integer"); }); });
        add("RecordTypeDefinition", [] { return make<RecordTypeDefinition>([](auto& n) {
                                            auto f = std::make_unique<RecordField>(); f->name = "x"; f->type = type("integer"); n.fields.push_back(std::move(f)); }); });

        // Declarations
        add("TypeDeclaration",      [] { return make<TypeDeclaration>([](auto& n) { n.name = "state"; n.definition = std::make_unique<EnumeratedTypeDefinition>(); }); });
        add("SubtypeDeclaration",   [] { return make<SubtypeDeclaration>([](auto& n) { n.name = "byte"; n.baseType = type("unsigned", range(7, 0)); }); });
        add("SignalDeclaration",    [] { return make<SignalDeclaration>([](auto& n) { n.name = "s"; n.typeSpec = type("bit"); n.initialValue = ch("'0'"); }); });
        add("ConstantDeclaration",  [] { return make<ConstantDeclaration>([](auto& n) { n.name = "k"; n.typeSpec = type("integer"); n.value = num(3); }); });
        add("VariableDeclaration",  [] { return make<VariableDeclaration>([](auto& n) { n.name = "v"; n.typeSpec = type("integer"); n.initialValue = num(3); }); });
        add("GenericDeclaration",   [] { return make<GenericDeclaration>([](auto& n) { n.name = "n"; n.typeSpec = type("natural"); n.defaultValue = num(8); }); });
        add("PortDeclaration",      [] { return make<PortDeclaration>([](auto& n) { n.name = "d"; n.typeSpec = type("bit"); n.mode = PortMode::InOut; n.defaultValue = ch("'0'"); }); });
        add("ParameterDeclaration", [] { return make<ParameterDeclaration>([](auto& n) {
                                            n.objectClass = ParameterClass::Signal; n.name = "s"; n.mode = PortMode::Out; n.typeSpec = type("bit"); n.defaultValue = ch("'1'"); }); });
        add("ComponentDeclaration", [] { return make<ComponentDeclaration>([](auto& n) {
                                            n.name = "adder";
                                            auto g = std::make_unique<GenericDeclaration>(); g->name = "n"; g->typeSpec = type("natural"); n.generics.push_back(std::move(g));
                                            auto p = std::make_unique<PortDeclaration>(); p->name = "a"; p->typeSpec = type("bit"); n.ports.push_back(std::move(p)); }); });
        add("LibraryClause",        [] { return make<LibraryClause>([](auto& n) { n.names = { "ieee", "work" }; }); });
        add("UseClause",            [] { return make<UseClause>([](auto& n) {
                                            auto f = std::make_unique<FieldAccessExpr>(); f->target = sym("ieee"); f->fieldName = "all";
                                            n.names.push_back(std::move(f)); }); });
        add("SubprogramSpec",       [] { return spec(); });
        add("SubprogramDeclaration",[] { return make<SubprogramDeclaration>([](auto& n) { n.spec = spec(); }); });
        add("SubprogramBody",       [] { return make<SubprogramBody>([](auto& n) {
                                            n.spec = spec();
                                            auto v = std::make_unique<VariableDeclaration>(); v->name = "t"; v->typeSpec = type("bit");
                                            n.declarations.push_back(std::move(v)); n.body = body(); }); });
        add("AliasDeclaration",     [] { return make<AliasDeclaration>([](auto& n) {
                                            n.name = "lo"; n.subtype = type("bit"); n.target = sym("v");
                                            n.signature = std::make_unique<SignatureExpr>(); n.signature->returnType = sym("bit"); }); });
        add("AttributeDeclaration", [] { return make<AttributeDeclaration>([](auto& n) { n.name = "keep"; n.typeMark = type("boolean"); }); });
        add("AttributeSpecification",[] { return make<AttributeSpecification>([](auto& n) {
                                            n.attributeName = "keep"; n.entities.push_back(sym("s")); n.entityClass = EntityClass::Label; n.value = sym("true"); }); });

        // Design units and root
        add("EntityDeclaration",    [] { return make<EntityDeclaration>([](auto& n) {
                                            n.name = "top";
                                            auto l = std::make_unique<LibraryClause>(); l->names = { "ieee" }; n.context.push_back(std::move(l));
                                            auto g = std::make_unique<GenericDeclaration>(); g->name = "n"; g->typeSpec = type("natural"); n.generics.push_back(std::move(g));
                                            auto p = std::make_unique<PortDeclaration>(); p->name = "a"; p->typeSpec = type("bit"); p->mode = PortMode::Out; n.ports.push_back(std::move(p)); }); });
        add("ArchitectureDeclaration",[] { return make<ArchitectureDeclaration>([](auto& n) {
                                            n.entityName = "top"; n.name = "rtl";
                                            auto l = std::make_unique<LibraryClause>(); l->names = { "work" }; n.context.push_back(std::move(l));
                                            auto s = std::make_unique<SignalDeclaration>(); s->name = "s"; s->typeSpec = type("bit"); n.declarations.push_back(std::move(s));
                                            n.body = body(); }); });
        add("ASTRoot",              [] { return make<ASTRoot>([](auto& n) {
                                            auto e = std::make_unique<EntityDeclaration>(); e->name = "top"; n.children.push_back(std::move(e));
                                            auto a = std::make_unique<ArchitectureDeclaration>(); a->name = "rtl"; a->entityName = "top"; a->body = body(); n.children.push_back(std::move(a)); }); });

        return nodes;
    }

    template <typename... Nodes>
    std::vector<std::unique_ptr<ASTNode>> defaultConstructed()
    {
        std::vector<std::unique_ptr<ASTNode>> nodes;
        (nodes.push_back(std::make_unique<Nodes>()), ...);
        return nodes;
    }

    /// Every concrete node type with nothing set.
    std::vector<std::unique_ptr<ASTNode>> emptyNodes()
    {
        return defaultConstructed<
            SymbolExpr, FieldAccessExpr, FunctionCallExpr, SignatureExpr, AttributeExpr, QualifiedExpr,
            NamedAssociationExpr, OpenExpr, ExternalNameExpr, TypeSpec,
            ElementResolutionExpr, IntegerLiteralExpr, DoubleLiteralExpr, PhysicalLiteralExpr, CharacterLiteralExpr,
            StringLiteralExpr, OthersExpr, AllExpr, ChoiceListExpr, AggregateExpr, BinaryOpExpr, UnaryOpExpr,
            WhenElseExpr, UnaffectedExpr,
            SignalAssignment, SelectedChoice, WithClause, ComponentInstantiation, ProcessStatement,
            VariableAssignment, IfBranch, IfStatement, CaseAlternative, CaseStatement, ForLoopStatement,
            WhileLoopStatement, LoopStatement, ExitStatement, NextStatement, NullStatement, WaitStatement,
            AssertStatement, ReportStatement, ReturnStatement, ProcedureCallStatement,
            NumericTypeDefinition, EnumeratedTypeDefinition, UnitDeclaration, PhysicalTypeDefinition,
            ArrayTypeDefinition, UnconstrainedArrayTypeDefinition, RecordField, RecordTypeDefinition,
            TypeDeclaration, SubtypeDeclaration, SignalDeclaration, ConstantDeclaration, VariableDeclaration,
            GenericDeclaration, PortDeclaration, ParameterDeclaration, ComponentDeclaration, LibraryClause,
            UseClause, SubprogramSpec, SubprogramDeclaration, SubprogramBody, AliasDeclaration,
            AttributeDeclaration, AttributeSpecification, EntityDeclaration, ArchitectureDeclaration, ASTRoot>();
    }

    /// Output of node.print(), captured by redirecting std::cout (works on every toolchain).
    std::string dump(const ASTNode& node)
    {
        std::ostringstream captured;
        std::streambuf* previous = std::cout.rdbuf(captured.rdbuf());
        node.print(0);
        std::cout.rdbuf(previous);
        return captured.str();
    }
}

// ===========================================================================
// 1. EVERY NODE TYPE
// ===========================================================================

TEST(AstClone, TableCoversEveryConcreteNodeOnce)
{
    auto nodes = populatedNodes();
    std::set<std::type_index> types;
    for (const auto& factory : nodes)
        types.insert(std::type_index(typeid(*factory.create())));

    EXPECT_EQ(types.size(), nodes.size()) << "two factories build the same node type";
    EXPECT_EQ(nodes.size(), 73u) << "a node was added or removed: update this table";
    EXPECT_EQ(emptyNodes().size(), nodes.size()) << "the empty-node list is out of sync with the table";
}

TEST(AstClone, KeepsDynamicTypeAndCreatesADistinctObject)
{
    for (const auto& factory : populatedNodes())
    {
        auto original = factory.create();
        auto copy = original->clone();

        ASSERT_NE(copy, nullptr) << factory.name;
        EXPECT_NE(copy.get(), original.get()) << factory.name;
        EXPECT_EQ(std::type_index(typeid(*copy)), std::type_index(typeid(*original))) << factory.name;
    }
}

TEST(AstClone, CopyPrintsExactlyLikeTheOriginal)
{
    for (const auto& factory : populatedNodes())
    {
        auto original = factory.create();
        auto copy = original->clone();

        EXPECT_EQ(dump(*copy), dump(*original)) << factory.name;
        EXPECT_FALSE(dump(*original).empty()) << factory.name;
    }
}

TEST(AstClone, CopyOfACopyIsStillEqual)
{
    for (const auto& factory : populatedNodes())
    {
        auto original = factory.create();
        auto twice = original->clone()->clone();

        EXPECT_EQ(dump(*twice), dump(*original)) << factory.name;
    }
}

TEST(AstClone, CopySurvivesTheOriginal)
{
    // A shallow copy would share (and after this, dangle on) the original's children.
    for (const auto& factory : populatedNodes())
    {
        auto original = factory.create();
        const std::string before = dump(*original);
        auto copy = original->clone();
        original.reset();

        EXPECT_EQ(dump(*copy), before) << factory.name;
    }
}

TEST(AstClone, PreservesSourceLocationAndLabel)
{
    for (const auto& factory : populatedNodes())
    {
        auto original = factory.create();
        original->source = { 12, 34 };
        if (auto* statement = dynamic_cast<Statement*>(original.get()))
            statement->label = "lbl";

        auto copy = original->clone();

        EXPECT_EQ(copy->source.line, 12u) << factory.name;
        EXPECT_EQ(copy->source.column, 34u) << factory.name;
        if (auto* statement = dynamic_cast<Statement*>(copy.get()))
        {
            EXPECT_EQ(statement->label, "lbl") << factory.name;
        }
    }
}

TEST(AstClone, DefaultConstructedNodesCloneWithNullChildren)
{
    // Optional children are null: cloning and printing must cope instead of crashing.
    for (const auto& node : emptyNodes())
    {
        auto copy = node->clone();

        ASSERT_NE(copy, nullptr);
        EXPECT_EQ(std::type_index(typeid(*copy)), std::type_index(typeid(*node)));
        EXPECT_EQ(dump(*copy), dump(*node));
    }

    auto whenElse = std::make_unique<WhenElseExpr>();
    whenElse->trueValue = sym("a");
    whenElse->condition = sym("c");
    auto copy = cloneOf(whenElse);
    EXPECT_NE(copy->trueValue, nullptr);
    EXPECT_EQ(copy->falseValue, nullptr) << "an omitted final else stays omitted";
}

// ===========================================================================
// 2. RECURSIVE SHAPES AND INDEPENDENCE
// ===========================================================================

TEST(AstClone, WhenElseChainIsClonedThroughFalseValue)
{
    // a when c1 else b when c2 else d
    auto inner = std::make_unique<WhenElseExpr>();
    inner->trueValue = sym("b");
    inner->condition = sym("c2");
    inner->falseValue = sym("d");

    auto outer = std::make_unique<WhenElseExpr>();
    outer->trueValue = sym("a");
    outer->condition = sym("c1");
    outer->falseValue = std::move(inner);

    auto copy = cloneOf(outer);

    auto* copyInner = dynamic_cast<WhenElseExpr*>(copy->falseValue.get());
    ASSERT_NE(copyInner, nullptr);
    EXPECT_NE(copyInner, dynamic_cast<WhenElseExpr*>(outer->falseValue.get()));
    EXPECT_EQ(dynamic_cast<SymbolExpr*>(copyInner->falseValue.get())->name, "d");
}

TEST(AstClone, MutatingTheOriginalDoesNotChangeTheCopy)
{
    auto original = std::make_unique<BinaryOpExpr>();
    original->op = BinaryOperator::Add;
    original->left = sym("a");
    original->right = sym("b");

    auto copy = cloneOf(original);

    original->op = BinaryOperator::Sub;
    dynamic_cast<SymbolExpr*>(original->left.get())->name = "changed";
    original->right.reset();

    EXPECT_EQ(copy->op, BinaryOperator::Add);
    EXPECT_EQ(dynamic_cast<SymbolExpr*>(copy->left.get())->name, "a");
    ASSERT_NE(copy->right, nullptr);
}

TEST(AstClone, IfStatementKeepsEveryBranchInOrder)
{
    auto original = std::make_unique<IfStatement>();
    for (int i = 0; i < 3; ++i)
    {
        auto branch = std::make_unique<IfBranch>();
        branch->condition = sym("c" + std::to_string(i));
        branch->body = body();
        original->branches.push_back(std::move(branch));
    }
    original->elseBody = body();

    auto copy = cloneOf(original);

    ASSERT_EQ(copy->branches.size(), 3u);
    for (size_t i = 0; i < 3; ++i)
        EXPECT_EQ(dynamic_cast<SymbolExpr*>(copy->branches[i]->condition.get())->name, "c" + std::to_string(i));
    EXPECT_EQ(copy->elseBody.size(), 2u);
}

TEST(AstClone, TypeSpecWithRangeInsideQualifiedExpression)
{
    auto spec = type("integer");
    spec->range = binary(BinaryOperator::To, num(0), num(3));

    auto qualified = std::make_unique<QualifiedExpr>();
    qualified->typeMark = std::move(spec);
    qualified->operand = num(1);

    auto copy = cloneOf(qualified);

    auto* copySpec = dynamic_cast<TypeSpec*>(copy->typeMark.get());
    ASSERT_NE(copySpec, nullptr);
    EXPECT_EQ(copySpec->typeName, "integer");
    EXPECT_NE(copySpec->range, nullptr);
    EXPECT_NE(copySpec, dynamic_cast<TypeSpec*>(qualified->typeMark.get()));
}

TEST(AstClone, CloneAllCopiesEveryElement)
{
    std::vector<ExpressionPtr> list;
    list.push_back(sym("a"));
    list.push_back(nullptr);
    list.push_back(num(1));

    auto copy = cloneAll(list);

    ASSERT_EQ(copy.size(), 3u);
    EXPECT_NE(copy[0], nullptr);
    EXPECT_NE(copy[0].get(), list[0].get());
    EXPECT_EQ(copy[1], nullptr);
    EXPECT_NE(copy[2], nullptr);
}

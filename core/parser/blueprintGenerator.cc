#include "blueprintGenerator.h"

#include <stdexcept>
#include <cassert>

namespace Pulse::Parser::VHDL
{
    // =========================================================================
    // Internal expression compiler
    // =========================================================================

    namespace
    {
        // -----------------------------------------------------------------------
        // Helpers
        // -----------------------------------------------------------------------

        std::unordered_map<std::string, Pulse::bitWidth_t>
        buildWidthTable(const LinkedArchitecture& arch)
        {
            std::unordered_map<std::string, Pulse::bitWidth_t> table;

            for (const auto& port : arch.targetEntity->ports)
                table[port->portName] = port->width;

            for (const SignalDeclaration* sig : arch.signals)
                table[sig->signalName] = sig->width;

            return table;
        }

        // -----------------------------------------------------------------------
        // ExprCompiler – recursive expression → blueprint component emitter
        // -----------------------------------------------------------------------

        struct ExprCompiler
        {
            Blueprint&                                              bp;
            const std::unordered_map<std::string, Pulse::bitWidth_t>& widths;
            int                                                     counter = 0;

            // ------------------------------------------------------------------
            // Allocate a fresh temporary wire and register it in the blueprint.
            // ------------------------------------------------------------------
            std::string makeTmp(Pulse::bitWidth_t width, const std::string& prefix = "$tmp_")
            {
                std::string name = prefix + std::to_string(counter++);
                bp.addSignal(name, width);
                return name;
            }

            // ------------------------------------------------------------------
            // Resolve the bit-width of an arbitrary LOGIC expression.
            // ------------------------------------------------------------------
            Pulse::bitWidth_t widthOf(const ASTNode* node)
            {
                if (auto lit = dynamic_cast<const LogicLiteralExpr*>(node))
                    return lit->width;

                if (auto ref = dynamic_cast<const SignalReference*>(node))
                {
                    if (!ref->low && !ref->high)
                    {
                        auto it = widths.find(ref->signalName);
                        if (it != widths.end()) return it->second;
                        throw std::runtime_error("BlueprintGenerator: unknown signal '" + ref->signalName + "'");
                    }
                    if (ref->low && !ref->high) return 1;
                    if (ref->low && ref->high)
                    {
                        auto lo = dynamic_cast<const IntegerLiteralExpr*>(ref->low.get());
                        auto hi = dynamic_cast<const IntegerLiteralExpr*>(ref->high.get());
                        if (lo && hi)
                            return static_cast<Pulse::bitWidth_t>(std::abs(hi->value - lo->value) + 1);
                    }
                    return 1;
                }

                if (auto bin = dynamic_cast<const BinaryOpExpr<ReturnType::LOGIC>*>(node))
                {
                    if (bin->op == "&")
                        return widthOf(bin->left.get()) + widthOf(bin->right.get());
                    return widthOf(bin->left.get());
                }

                if (auto un = dynamic_cast<const UnaryOpExpr<ReturnType::LOGIC>*>(node))
                    return widthOf(un->operand.get());

                if (auto we = dynamic_cast<const WhenElseExpr*>(node))
                    return widthOf(we->defaultValue.get());

                throw std::runtime_error("BlueprintGenerator: cannot determine width of expression node.");
            }

            // ------------------------------------------------------------------
            // Main recursive compile entry point.
            // If targetWire is non-empty, components output directly into it.
            // ------------------------------------------------------------------
            std::string compile(const ASTNode* node, const std::string& targetWire = "")
            {
                // ── Constant literal ────────────────────────────────────────────
                if (auto lit = dynamic_cast<const LogicLiteralExpr*>(node))
                {
                    std::string out = targetWire.empty() ? makeTmp(lit->width, "$const_") : targetWire;
                    LogicVector val{ lit->value, lit->unknownMask };
                    bp.addComponent("$cst_" + std::to_string(counter++),
                                    std::make_unique<ConstantInstance>(out, std::move(val)));
                    return out;
                }

                // ── Signal reference (possibly sliced) ─────────────────────────
                if (auto ref = dynamic_cast<const SignalReference*>(node))
                    return compileSignalRef(ref, targetWire);

                // ── Unary operation ────────────────────────────────────────────
                if (auto un = dynamic_cast<const UnaryOpExpr<ReturnType::LOGIC>*>(node))
                    return compileUnary(un, targetWire);

                // ── Binary operation ───────────────────────────────────────────
                if (auto bin = dynamic_cast<const BinaryOpExpr<ReturnType::LOGIC>*>(node))
                    return compileBinary(bin, targetWire);

                // ── When/Else expression ───────────────────────────────────────
                if (auto we = dynamic_cast<const WhenElseExpr*>(node))
                    return compileWhenElse(we, targetWire);

                throw std::runtime_error("BlueprintGenerator: unrecognised expression node type.");
            }

            // ------------------------------------------------------------------
            // Signal reference  (full / single-bit / range)
            // ------------------------------------------------------------------
            std::string compileSignalRef(const SignalReference* ref, const std::string& targetWire = "")
            {
                const std::string& base = ref->signalName;

                // Full signal reference
                if (!ref->low && !ref->high)
                {
                    if (!targetWire.empty() && targetWire != base)
                    {
                        bp.addComponent("$join_" + base + "_" + std::to_string(counter++),
                                        std::make_unique<JoinInstance>(base, targetWire));
                        return targetWire;
                    }
                    return base;
                }

                // Single-bit index
                if (ref->low && !ref->high)
                {
                    auto idxExpr = dynamic_cast<const IntegerLiteralExpr*>(ref->low.get());
                    if (!idxExpr)
                        throw std::runtime_error("BlueprintGenerator: dynamic bit-index not supported.");

                    auto it = widths.find(base);
                    if (it == widths.end())
                        throw std::runtime_error("BlueprintGenerator: unknown signal '" + base + "'");

                    auto bit = static_cast<Pulse::bitWidth_t>(idxExpr->value);
                    std::string out = targetWire.empty() ? makeTmp(1, "$split_") : targetWire;
                    bp.addComponent("$sp_" + std::to_string(counter++),
                                    std::make_unique<SplitterInstance>(base, out, bit, bit));
                    return out;
                }

                // Range slice
                {
                    auto loExpr = dynamic_cast<const IntegerLiteralExpr*>(ref->low.get());
                    auto hiExpr = dynamic_cast<const IntegerLiteralExpr*>(ref->high.get());
                    if (!loExpr || !hiExpr)
                        throw std::runtime_error("BlueprintGenerator: dynamic range slice not supported.");

                    auto lo = static_cast<Pulse::bitWidth_t>(loExpr->value);
                    auto hi = static_cast<Pulse::bitWidth_t>(hiExpr->value);
                    Pulse::bitWidth_t w = static_cast<Pulse::bitWidth_t>(std::abs((int)hi - (int)lo) + 1);
                    std::string out = targetWire.empty() ? makeTmp(w, "$split_") : targetWire;
                    bp.addComponent("$sp_" + std::to_string(counter++),
                                    std::make_unique<SplitterInstance>(base, out, hi, lo));
                    return out;
                }
            }

            // ------------------------------------------------------------------
            // Unary operation
            // ------------------------------------------------------------------
            std::string compileUnary(const UnaryOpExpr<ReturnType::LOGIC>* un, const std::string& targetWire = "")
            {
                std::string in  = compile(un->operand.get());
                Pulse::bitWidth_t w = widthOf(un->operand.get());
                std::string out = targetWire.empty() ? makeTmp(w) : targetWire;

                if (un->op == "not")
                {
                    bp.addComponent("$not_" + std::to_string(counter++),
                                    std::make_unique<NotGateInstance>(in, out));
                }
                else
                {
                    throw std::runtime_error("BlueprintGenerator: unsupported unary op '" + un->op + "'.");
                }
                return out;
            }

            // ------------------------------------------------------------------
            // Binary operation
            // ------------------------------------------------------------------
            std::string compileBinary(const BinaryOpExpr<ReturnType::LOGIC>* bin, const std::string& targetWire = "")
            {
                std::string lhs = compile(bin->left.get());
                std::string rhs = compile(bin->right.get());

                // ── Logical / bitwise ──────────────────────────────────────────
                static const struct { const char* op; Pulse::Engine::BinaryOp bop; }
                logicOps[] = {
                    { "and",  Pulse::Engine::BinaryOp::AND  },
                    { "or",   Pulse::Engine::BinaryOp::OR   },
                    { "xor",  Pulse::Engine::BinaryOp::XOR  },
                    { "nand", Pulse::Engine::BinaryOp::NAND },
                    { "nor",  Pulse::Engine::BinaryOp::NOR  },
                    { "xnor", Pulse::Engine::BinaryOp::XNOR },
                };
                for (auto& entry : logicOps)
                {
                    if (bin->op == entry.op)
                    {
                        Pulse::bitWidth_t w = widthOf(bin->left.get());
                        std::string out = targetWire.empty() ? makeTmp(w) : targetWire;
                        bp.addComponent("$gate_" + std::to_string(counter++),
                                        std::make_unique<BinaryGateInstance>(lhs, rhs, out, entry.bop));
                        return out;
                    }
                }

                // ── Concatenation ─────────────────────────────────────────────
                if (bin->op == "&")
                {
                    Pulse::bitWidth_t w = widthOf(bin->left.get()) + widthOf(bin->right.get());
                    std::string out = targetWire.empty() ? makeTmp(w) : targetWire;
                    bp.addComponent("$cat_" + std::to_string(counter++),
                                    std::make_unique<ConcatenatorInstance>(rhs, lhs, out));
                    return out;
                }

                // ── Shift operations ───────────────────────────────────────────
                static const struct { const char* op; Pulse::Engine::ShiftOp sop; }
                shiftOps[] = {
                    { "sll", Pulse::Engine::ShiftOp::LogicalLeft    },
                    { "srl", Pulse::Engine::ShiftOp::LogicalRight   },
                    { "sra", Pulse::Engine::ShiftOp::ArithmeticRight },
                    { "rol", Pulse::Engine::ShiftOp::RotateLeft     },
                    { "ror", Pulse::Engine::ShiftOp::RotateRight    },
                };
                for (auto& entry : shiftOps)
                {
                    if (bin->op == entry.op)
                    {
                        Pulse::bitWidth_t w = widthOf(bin->left.get());
                        std::string out = targetWire.empty() ? makeTmp(w) : targetWire;
                        bp.addComponent("$shft_" + std::to_string(counter++),
                                        std::make_unique<ShifterInstance>(lhs, rhs, out, entry.sop));
                        return out;
                    }
                }

                // ── Arithmetic ────────────────────────────────────────────────
                {
                    Pulse::bitWidth_t w = widthOf(bin->left.get());
                    std::string out = targetWire.empty() ? makeTmp(w) : targetWire;
                    if (bin->op == "+")
                    {
                        bp.addComponent("$add_" + std::to_string(counter++),
                                        std::make_unique<AdderInstance>(lhs, rhs, out));
                        return out;
                    }
                    if (bin->op == "-")
                    {
                        bp.addComponent("$sub_" + std::to_string(counter++),
                                        std::make_unique<SubtractorInstance>(lhs, rhs, out));
                        return out;
                    }
                    if (bin->op == "*")
                    {
                        bp.addComponent("$mul_" + std::to_string(counter++),
                                        std::make_unique<MultiplicatorInstance>(lhs, rhs, out));
                        return out;
                    }
                }

                throw std::runtime_error("BlueprintGenerator: unsupported binary op '" + bin->op + "'.");
            }

            // ------------------------------------------------------------------
            // When/Else  →  chain of ControlledBuffer instances
            // ------------------------------------------------------------------
            std::string compileWhenElse(const WhenElseExpr* we, const std::string& targetWire = "")
            {
                Pulse::bitWidth_t w = widthOf(we->defaultValue.get());
                std::string finalOut = targetWire.empty() ? makeTmp(w, "$muxout_") : targetWire;

                // Keep track of whether ANY previous condition was true
                std::string anyPrevCond = makeTmp(1, "$any_prev_");
                bp.addComponent("$cst_zero_" + std::to_string(counter++),
                                std::make_unique<ConstantInstance>(anyPrevCond, LogicVector{0, 0}));

                // Process each conditional branch
                for (const auto& branch : we->branches)
                {
                    // Compile the condition into a 1-bit wire
                    std::string rawCond = compileCondition(branch.condition.get());
                    std::string valWire = compile(branch.value.get());

                    // Enable this branch only if its condition is true AND no previous condition was true
                    std::string notPrev = makeTmp(1, "$not_prev_");
                    bp.addComponent("$not_" + std::to_string(counter++),
                                    std::make_unique<NotGateInstance>(anyPrevCond, notPrev));
                    
                    std::string actualCond = makeTmp(1, "$actual_cond_");
                    bp.addComponent("$and_" + std::to_string(counter++),
                                    std::make_unique<BinaryGateInstance>(rawCond, notPrev, actualCond, Pulse::Engine::BinaryOp::AND));

                    // Gate the value through a controlled buffer, driving finalOut directly
                    bp.addComponent("$cbuf_" + std::to_string(counter++),
                                    std::make_unique<ControlledBufferInstance>(valWire, finalOut, actualCond));

                    // Update anyPrevCond = anyPrevCond OR rawCond
                    std::string nextAny = makeTmp(1, "$any_prev_next_");
                    bp.addComponent("$or_" + std::to_string(counter++),
                                    std::make_unique<BinaryGateInstance>(anyPrevCond, rawCond, nextAny, Pulse::Engine::BinaryOp::OR));
                    anyPrevCond = nextAny;
                }

                // Compile the default value
                std::string defWire = compile(we->defaultValue.get());

                // Condition for default value is NOT anyPrevCond
                std::string defCond = makeTmp(1, "$def_cond_");
                bp.addComponent("$not_def_" + std::to_string(counter++),
                                std::make_unique<NotGateInstance>(anyPrevCond, defCond));

                // Gate the default value and drive finalOut
                bp.addComponent("$cbuf_def_" + std::to_string(counter++),
                                std::make_unique<ControlledBufferInstance>(defWire, finalOut, defCond));

                return finalOut;
            }

            // ------------------------------------------------------------------
            // Condition expression  →  1-bit wire
            // ------------------------------------------------------------------
            std::string compileCondition(const ASTNode* node, const std::string& targetWire = "")
            {
                if (auto bin = dynamic_cast<const BinaryOpExpr<ReturnType::BOOLEAN>*>(node))
                {
                    if (bin->op == "and" || bin->op == "or")
                    {
                        std::string lhs = compileCondition(bin->left.get());
                        std::string rhs = compileCondition(bin->right.get());
                        std::string out = targetWire.empty() ? makeTmp(1, "$cond_") : targetWire;
                        auto bop = (bin->op == "and") ? Pulse::Engine::BinaryOp::AND
                                                      : Pulse::Engine::BinaryOp::OR;
                        bp.addComponent("$cgate_" + std::to_string(counter++),
                                        std::make_unique<BinaryGateInstance>(lhs, rhs, out, bop));
                        return out;
                    }

                    static const struct { const char* op; Pulse::Engine::CompareOp cop; }
                    cmpOps[] = {
                        { "=",  Pulse::Engine::CompareOp::Equals           },
                        { "/=", Pulse::Engine::CompareOp::NotEquals        },
                        { "<",  Pulse::Engine::CompareOp::LessThan         },
                        { "<=", Pulse::Engine::CompareOp::LessThanEqual    },
                        { ">",  Pulse::Engine::CompareOp::GreaterThan      },
                        { ">=", Pulse::Engine::CompareOp::GreaterThanEqual },
                    };
                    for (auto& entry : cmpOps)
                    {
                        if (bin->op == entry.op)
                        {
                            std::string lhs = compile(bin->left.get());
                            std::string rhs = compile(bin->right.get());
                            std::string out = targetWire.empty() ? makeTmp(1, "$cmp_") : targetWire;
                            bp.addComponent("$cmp_" + std::to_string(counter++),
                                            std::make_unique<ComparatorInstance>(
                                                lhs, rhs, out, entry.cop,
                                                Pulse::Engine::CompareMode::Unsigned));
                            return out;
                        }
                    }

                    throw std::runtime_error("BlueprintGenerator: unsupported boolean op '" + bin->op + "'.");
                }

                if (auto un = dynamic_cast<const UnaryOpExpr<ReturnType::BOOLEAN>*>(node))
                {
                    if (un->op == "not")
                    {
                        std::string in  = compileCondition(un->operand.get());
                        std::string out = targetWire.empty() ? makeTmp(1, "$cnot_") : targetWire;
                        bp.addComponent("$cnot_" + std::to_string(counter++),
                                        std::make_unique<NotGateInstance>(in, out));
                        return out;
                    }
                    throw std::runtime_error("BlueprintGenerator: unsupported unary boolean op '" + un->op + "'.");
                }

                return compile(node, targetWire);
            }

            // ------------------------------------------------------------------
            // Helper to add a port to a port list without duplicates
            // ------------------------------------------------------------------
            static void addPortIfMissing(std::vector<std::string>& ports, const std::string& name)
            {
                if (std::find(ports.begin(), ports.end(), name) == ports.end())
                {
                    ports.push_back(name);
                }
            }

            // ------------------------------------------------------------------
            // Compile sequential statements in a process body into ProcessInstructions
            // ------------------------------------------------------------------
            void compileSequentialStatements(
                const std::vector<std::unique_ptr<SequentialStatement>>& stmts,
                std::vector<std::unique_ptr<Pulse::Engine::ProcessInstruction>>& outInstructions,
                std::vector<std::string>& inPorts,
                std::vector<std::string>& outPorts)
            {
                for (const auto& stmt : stmts)
                {
                    if (auto asgn = dynamic_cast<const SignalAssignment*>(stmt.get()))
                    {
                        const SignalReference* tgt = asgn->target.get();
                        std::string valWire = compile(asgn->value.get());
                        std::string tgtName = tgt->signalName;

                        auto inst = std::make_unique<Pulse::Engine::ProcessInstructionAssignment>();
                        inst->sourcePort = valWire;
                        inst->targetPort = tgtName;

                        addPortIfMissing(inPorts, valWire);
                        addPortIfMissing(outPorts, tgtName);

                        outInstructions.push_back(std::move(inst));
                    }
                    else if (auto waitStmt = dynamic_cast<const WaitForStatement*>(stmt.get()))
                    {
                        auto inst = std::make_unique<Pulse::Engine::ProcessInstructionWait>();
                        inst->waitTime = waitStmt->durationFs;
                        outInstructions.push_back(std::move(inst));
                    }
                    else if (auto waitForeverStmt = dynamic_cast<const WaitForeverStatement*>(stmt.get()))
                    {
                        auto inst = std::make_unique<Pulse::Engine::ProcessInstructionWaitForever>();
                        outInstructions.push_back(std::move(inst));
                    }
                    else if (auto ifStmt = dynamic_cast<const IfStatement*>(stmt.get()))
                    {
                        compileIfStatement(ifStmt, outInstructions, inPorts, outPorts);
                    }
                    else
                    {
                        throw std::runtime_error("BlueprintGenerator: unsupported sequential statement in process.");
                    }
                }
            }

            // ------------------------------------------------------------------
            // Compile IfStatement (if / elsif / else)
            // ------------------------------------------------------------------
            void compileIfStatement(
                const IfStatement* ifStmt,
                std::vector<std::unique_ptr<Pulse::Engine::ProcessInstruction>>& outInstructions,
                std::vector<std::string>& inPorts,
                std::vector<std::string>& outPorts)
            {
                struct CompiledBranch
                {
                    std::string notCondWire;
                    std::vector<std::unique_ptr<Pulse::Engine::ProcessInstruction>> instructions;
                };

                std::vector<CompiledBranch> compiledBranches;
                compiledBranches.reserve(ifStmt->branches.size());

                for (const auto& branch : ifStmt->branches)
                {
                    std::string condWire = compileCondition(branch.condition.get());
                    addPortIfMissing(inPorts, condWire);

                    std::vector<std::unique_ptr<Pulse::Engine::ProcessInstruction>> branchInsts;
                    compileSequentialStatements(branch.body, branchInsts, inPorts, outPorts);

                    compiledBranches.push_back({ condWire, std::move(branchInsts) });
                }

                std::vector<std::unique_ptr<Pulse::Engine::ProcessInstruction>> compiledElse;
                if (!ifStmt->elseBody.empty())
                {
                    compileSequentialStatements(ifStmt->elseBody, compiledElse, inPorts, outPorts);
                }

                size_t numBranches = compiledBranches.size();
                size_t elseSize = compiledElse.size();

                // Compute block sizes and suffix sizes for jump calculations
                std::vector<size_t> blockSize(numBranches, 0);
                for (size_t i = 0; i < numBranches; ++i)
                {
                    bool hasFollowing = (i + 1 < numBranches) || (elseSize > 0);
                    blockSize[i] = 1 + compiledBranches[i].instructions.size() + (hasFollowing ? 1 : 0);
                }

                std::vector<size_t> suffixSize(numBranches + 1, 0);
                suffixSize[numBranches] = elseSize;
                for (size_t i = numBranches; i > 0; --i)
                {
                    suffixSize[i - 1] = blockSize[i - 1] + suffixSize[i];
                }

                // Emit instructions
                for (size_t i = 0; i < numBranches; ++i)
                {
                    bool hasFollowing = (i + 1 < numBranches) || (elseSize > 0);

                    // 1. Conditional branch: if condition is false (notCond is 1), skip this branch's body (+ exit jump if present)
                    auto condBranch = std::make_unique<Pulse::Engine::ProcessInstructionBranch>();
                    condBranch->conditionPort = compiledBranches[i].notCondWire;
                    condBranch->branchLength = compiledBranches[i].instructions.size() + (hasFollowing ? 1 : 0);
                    outInstructions.push_back(std::move(condBranch));

                    // 2. Branch body
                    for (auto& inst : compiledBranches[i].instructions)
                    {
                        outInstructions.push_back(std::move(inst));
                    }

                    // 3. Unconditional exit jump to skip to the end of the entire if/elsif/else
                    if (hasFollowing)
                    {
                        auto exitJump = std::make_unique<Pulse::Engine::ProcessInstructionBranchAlways>();
                        exitJump->branchLength = suffixSize[i + 1];
                        outInstructions.push_back(std::move(exitJump));
                    }
                }

                // 4. Else body
                for (auto& inst : compiledElse)
                {
                    outInstructions.push_back(std::move(inst));
                }
            }
        };

    } // anonymous namespace

    // =========================================================================
    // BlueprintGenerator::generate
    // =========================================================================

    std::unordered_map<BlueprintGenerator::EntityName, std::unique_ptr<Blueprint>>
    BlueprintGenerator::generate(const LinkedDesign& design, std::string architectureName)
    {
        std::unordered_map<EntityName, std::unique_ptr<Blueprint>> result;

        for (const LinkedArchitecture& arch : design.architectures)
        {
            if (arch.architectureName != architectureName)
                continue;

            const EntityDeclaration* entity = arch.targetEntity;
            const std::string& entityName   = entity->entityName;

            if (result.count(entityName))
                continue;

            auto bp = std::make_unique<Blueprint>();

            // 1. Register entity ports
            for (const auto& port : entity->ports)
                bp->addPort(port->portName, port->isInput);
            
            // 2. Register internal signals
            for (const SignalDeclaration* sig : arch.signals)
                bp->addSignal(sig->signalName, sig->width);

            // 3. Compile concurrent signal assignments
            auto widthTable = buildWidthTable(arch);
            ExprCompiler compiler{ *bp, widthTable, 0 };

            for (const SignalAssignment* asgn : arch.assignments)
            {
                const SignalReference* tgt = asgn->target.get();

                if (!tgt->low && !tgt->high)
                {
                    // Full assignment: directly drive target wire
                    compiler.compile(asgn->value.get(), tgt->signalName);
                }
                else
                {
                    // Partial assignment write-back
                    std::string resultWire = compiler.compile(asgn->value.get());
                    bp->addComponent(
                        "$join_" + tgt->signalName + "_slice_" + std::to_string(compiler.counter++),
                        std::make_unique<JoinInstance>(resultWire, tgt->signalName));
                }
            }

            // 4. Emit component instantiations as SubgraphInstances
            for (const ResolvedInstantiation& inst : arch.resolvedInstantiations)
            {
                std::unordered_map<std::string, std::string> portMap;

                for (const auto& [portName, exprNode] : inst.portBindings)
                {
                    if (auto ref = dynamic_cast<const SignalReference*>(exprNode))
                    {
                        if (!ref->low && !ref->high)
                        {
                            portMap[portName] = ref->signalName;
                        }
                        else
                        {
                            std::string sliceWire = compiler.compileSignalRef(ref);
                            portMap[portName] = sliceWire;
                        }
                    }
                    else
                    {
                        std::string exprWire = compiler.compile(exprNode);
                        portMap[portName] = exprWire;
                    }
                }

                bp->addComponent(inst.instanceName,
                                 std::make_unique<SubgraphInstance>(nullptr, std::move(portMap)));
            }

            // 5. Compile processes
            for (const ProcessStatement* proc : arch.processes)
            {
                std::vector<std::string> inPorts;
                std::vector<std::string> outPorts;
                std::vector<std::unique_ptr<Pulse::Engine::ProcessInstruction>> instructions;

                compiler.compileSequentialStatements(proc->body, instructions, inPorts, outPorts);

                std::string procName = proc->label.empty()
                    ? ("$proc_" + std::to_string(compiler.counter++))
                    : proc->label;

                bp->addComponent(
                    procName,
                    std::make_unique<ProcessInstance>(
                        std::move(inPorts),
                        std::move(outPorts),
                        std::move(instructions),
                        std::move(proc->sensitivityList)
                    )
                );
            }

            result[entityName] = std::move(bp);
        }

        // Pass 2: Patch SubgraphInstance::bp raw pointers
        std::unordered_map<std::string, const LinkedArchitecture*> archByEntity;
        for (const LinkedArchitecture& arch : design.architectures)
        {
            if (arch.architectureName == architectureName)
                archByEntity[arch.targetEntity->entityName] = &arch;
        }

        for (auto& [entityName, bp] : result)
        {
            auto archIt = archByEntity.find(entityName);
            if (archIt == archByEntity.end()) continue;

            const LinkedArchitecture& arch = *archIt->second;

            for (const ResolvedInstantiation& inst : arch.resolvedInstantiations)
            {
                auto compIt = bp->components.find(inst.instanceName);
                if (compIt == bp->components.end()) continue;

                auto* subgraph = dynamic_cast<SubgraphInstance*>(compIt->second.get());
                if (!subgraph) continue;

                const std::string& targetEntityName = inst.targetEntity->entityName;
                auto bpIt = result.find(targetEntityName);
                if (bpIt != result.end())
                    subgraph->bp = bpIt->second.get();
            }
        }

        return result;
    }

} // namespace Pulse::Parser::VHDL
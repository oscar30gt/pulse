#include "blueprintGenerator.h"
#include <stdexcept>
#include <algorithm>
#include <cctype>

namespace Pulse::Parser::VHDL
{
    std::unordered_map<std::string, std::unique_ptr<Pulse::Parser::Blueprint>> 
    BlueprintGenerator::generate(const LinkedDesign& design)
    {
        std::unordered_map<std::string, std::unique_ptr<Pulse::Parser::Blueprint>> registry;
        std::unordered_map<std::string, Pulse::Parser::Blueprint*> entityToBlueprint;

        // Pass 1: Allocate blueprints and register ports/signals
        for (const auto& arch : design.architectures)
        {
            auto bp = std::make_unique<Pulse::Parser::Blueprint>();

            // Map target entity ports to blueprint
            if (arch.targetEntity)
            {
                for (const auto& port : arch.targetEntity->ports)
                {
                    bp->addPort(port->portName, port->isInput);
                    bp->addSignal(port->portName, port->width);
                }
                // For subgraph linking, map entity name to its blueprint
                entityToBlueprint[arch.targetEntity->entityName] = bp.get();
            }

            // Map internal signals to blueprint
            for (const auto* sig : arch.signals)
            {
                bp->addSignal(sig->signalName, sig->width);
            }

            registry[arch.architectureName] = std::move(bp);
        }

        // Pass 2: Populate components, assignments, and resolve subgraphs
        for (const auto& arch : design.architectures)
        {
            Pulse::Parser::Blueprint* bp = registry[arch.architectureName].get();
            m_tempWireCounter = 0;
            m_compCounter = 0;

            // 1. Process Concurrent Assignments
            for (const auto* assign : arch.assignments)
            {
                if (assign && assign->target)
                {
                    // Build the expression tree, routing the final output to the target signal
                    buildExpression(assign->value.get(), *bp, assign->target->signalName);
                }
            }

            // 2. Process Resolved Instantiations (Subgraphs)
            for (const auto& inst : arch.resolvedInstantiations)
            {
                Pulse::Parser::Blueprint* targetSubBp = nullptr;
                if (inst.targetEntity)
                {
                    targetSubBp = entityToBlueprint[inst.targetEntity->entityName];
                }

                // Flatten expressions used in port mappings
                std::unordered_map<std::string, std::string> mappedPorts;
                for (const auto& [portName, exprNode] : inst.portBindings)
                {
                    // Build the expression and grab the wire holding its result
                    std::string wireName = buildExpression(exprNode, *bp, "");
                    mappedPorts[portName] = wireName;
                }

                bp->addComponent(
                    inst.instanceName, 
                    std::make_unique<Pulse::Parser::SubgraphInstance>(targetSubBp, mappedPorts)
                );
            }
        }

        return registry;
    }

    std::string BlueprintGenerator::genTempWire(Pulse::Parser::Blueprint& bp, bitWidth_t width)
    {
        std::string name = "$wire_" + std::to_string(m_tempWireCounter++);
        bp.addSignal(name, width);
        return name;
    }

    std::string BlueprintGenerator::genCompName(const std::string& prefix)
    {
        return "$" + prefix + "_" + std::to_string(m_compCounter++);
    }

    std::string BlueprintGenerator::buildExpression(const ASTNode* node, Pulse::Parser::Blueprint& bp, const std::string& targetWire)
    {
        if (!node) return "";

        // --- 1. Signal References ---
        if (auto ref = dynamic_cast<const SignalReference*>(node))
        {
            std::string baseWire = ref->signalName;

            // Handle slicing: A(5 downto 2) -> SplitterInstance
            if (ref->high && ref->low)
            {
                bitWidth_t highIdx = evaluateStaticInteger(ref->high.get());
                bitWidth_t lowIdx  = evaluateStaticInteger(ref->low.get());

                std::string outWire = targetWire.empty() ? genTempWire(bp, highIdx - lowIdx + 1) : targetWire;
                
                bp.addComponent(genCompName("split"), 
                    std::make_unique<Pulse::Parser::SplitterInstance>(baseWire, outWire, highIdx, lowIdx));
                return outWire;
            }

            // If it's a direct signal but we need to assign it to targetWire (e.g., A <= B)
            if (!targetWire.empty() && baseWire != targetWire)
            {
                bp.addComponent(genCompName("join"), 
                    std::make_unique<Pulse::Parser::JoinInstance>(baseWire, targetWire));
                return targetWire;
            }

            return baseWire;
        }

        // --- 2. Logic & Integer Literals ---
        if (auto lit = dynamic_cast<const LogicLiteralExpr*>(node))
        {
            std::string outWire = targetWire.empty() ? genTempWire(bp, lit->width) : targetWire;
            
            // Create a ConstantInstance to drive the wire instead of returning the raw value string
            // Requires LogicVector to accept the literal value (and potentially width/mask)
            bp.addComponent(genCompName("const"), 
                std::make_unique<Pulse::Parser::ConstantInstance>(outWire, lit->value));
            
            return outWire;
        }

        // --- 3. When Else Expressions ---
        if (auto whenElse = dynamic_cast<const WhenElseExpr*>(node))
        {
            std::string outWire = targetWire.empty() ? genTempWire(bp, 0) : targetWire;
            std::string aggregatedCondWire = ""; 
            
            for (const auto& branch : whenElse->branches)
            {
                std::string condWire = buildExpression(branch.condition.get(), bp, "");
                std::string valWire = buildExpression(branch.value.get(), bp, "");
                
                // Add controlled buffer for the evaluated branch
                bp.addComponent(genCompName("c_buf"), 
                    std::make_unique<Pulse::Parser::ControlledBufferInstance>(valWire, outWire, condWire));

                // Accumulate the used conditions (NOR'd later) if a default branch exists
                if (whenElse->defaultValue)
                {
                    if (aggregatedCondWire.empty())
                    {
                        aggregatedCondWire = condWire;
                    }
                    else
                    {
                        std::string nextAggWire = genTempWire(bp, 1);
                        bp.addComponent(genCompName("or"), 
                            std::make_unique<Pulse::Parser::BinaryGateInstance>(
                                aggregatedCondWire, condWire, nextAggWire, Pulse::Engine::BinaryOp::OR));
                        aggregatedCondWire = nextAggWire;
                    }
                }
            }

            if (whenElse->defaultValue)
            {
                std::string defValWire = buildExpression(whenElse->defaultValue.get(), bp, "");
                std::string defaultEnableWire = genTempWire(bp, 1);
                
                // Default enable is active only if NO other branches triggered
                bp.addComponent(genCompName("not"), 
                    std::make_unique<Pulse::Parser::NotGateInstance>(aggregatedCondWire, defaultEnableWire));

                bp.addComponent(genCompName("c_buf_def"), 
                    std::make_unique<Pulse::Parser::ControlledBufferInstance>(defValWire, outWire, defaultEnableWire));
            }

            return outWire;
        }

        // --- 4. Binary Operations (LOGIC) ---
        if (auto binOp = dynamic_cast<const BinaryOpExpr<ReturnType::LOGIC>*>(node))
        {
            std::string leftWire = buildExpression(binOp->left.get(), bp, "");
            std::string rightWire = buildExpression(binOp->right.get(), bp, "");
            std::string outWire = targetWire.empty() ? genTempWire(bp, 0) : targetWire;

            // Make operator parsing case insensitive
            std::string opStr = binOp->op;
            std::transform(opStr.begin(), opStr.end(), opStr.begin(), ::toupper);

            if (opStr == "AND" || opStr == "OR" || opStr == "XOR" || 
                opStr == "NAND" || opStr == "NOR" || opStr == "XNOR")
            {
                Pulse::Engine::BinaryOp eOp = Pulse::Engine::BinaryOp::AND;
                if (opStr == "OR")        eOp = Pulse::Engine::BinaryOp::OR;
                else if (opStr == "XOR")  eOp = Pulse::Engine::BinaryOp::XOR;
                else if (opStr == "NAND") eOp = Pulse::Engine::BinaryOp::NAND;
                else if (opStr == "NOR")  eOp = Pulse::Engine::BinaryOp::NOR;
                else if (opStr == "XNOR") eOp = Pulse::Engine::BinaryOp::XNOR;

                bp.addComponent(genCompName("gate"), 
                    std::make_unique<Pulse::Parser::BinaryGateInstance>(leftWire, rightWire, outWire, eOp));
            }
            else if (opStr == "+")
            {
                bp.addComponent(genCompName("add"), 
                    std::make_unique<Pulse::Parser::AdderInstance>(leftWire, rightWire, outWire));
            }
            else if (opStr == "-")
            {
                bp.addComponent(genCompName("sub"), 
                    std::make_unique<Pulse::Parser::SubtractorInstance>(leftWire, rightWire, outWire));
            }
            else if (opStr == "*")
            {
                bp.addComponent(genCompName("mul"), 
                    std::make_unique<Pulse::Parser::MultiplicatorInstance>(leftWire, rightWire, outWire));
            }
            else if (opStr == "&") // Concatenation
            {
                bp.addComponent(genCompName("concat"), 
                    std::make_unique<Pulse::Parser::ConcatenatorInstance>(rightWire, leftWire, outWire));
            }
            else if (opStr == "SLL" || opStr == "SRL" || opStr == "SRA" || opStr == "ROL" || opStr == "ROR")
            {
                Pulse::Engine::ShiftOp sOp = Pulse::Engine::ShiftOp::LogicalLeft;
                if (opStr == "SRL")       sOp = Pulse::Engine::ShiftOp::LogicalRight;
                else if (opStr == "SRA")  sOp = Pulse::Engine::ShiftOp::ArithmeticRight;
                else if (opStr == "ROL")  sOp = Pulse::Engine::ShiftOp::RotateLeft;
                else if (opStr == "ROR")  sOp = Pulse::Engine::ShiftOp::RotateRight;

                bp.addComponent(genCompName("shift"),
                    std::make_unique<Pulse::Parser::ShifterInstance>(leftWire, rightWire, outWire, sOp));
            }

            return outWire;
        }

        // --- 5. Binary Operations (BOOLEAN) ---
        if (auto binOpBool = dynamic_cast<const BinaryOpExpr<ReturnType::BOOLEAN>*>(node))
        {
            std::string leftWire = buildExpression(binOpBool->left.get(), bp, "");
            std::string rightWire = buildExpression(binOpBool->right.get(), bp, "");
            std::string outWire = targetWire.empty() ? genTempWire(bp, 1) : targetWire;

            Pulse::Engine::CompareOp cOp = Pulse::Engine::CompareOp::Equals;
            std::string opStr = binOpBool->op;

            if (opStr == "=")       cOp = Pulse::Engine::CompareOp::Equals;
            else if (opStr == "/=") cOp = Pulse::Engine::CompareOp::NotEquals;
            else if (opStr == "<")  cOp = Pulse::Engine::CompareOp::LessThan;
            else if (opStr == "<=") cOp = Pulse::Engine::CompareOp::LessThanEqual;
            else if (opStr == ">")  cOp = Pulse::Engine::CompareOp::GreaterThan;
            else if (opStr == ">=") cOp = Pulse::Engine::CompareOp::GreaterThanEqual;

            bp.addComponent(genCompName("comp"), 
                std::make_unique<Pulse::Parser::ComparatorInstance>(
                    leftWire, rightWire, outWire, cOp, Pulse::Engine::CompareMode::Unsigned));

            return outWire;
        }

        // --- 6. Unary Operations ---
        if (auto unOp = dynamic_cast<const UnaryOpExpr<ReturnType::LOGIC>*>(node))
        {
            std::string operandWire = buildExpression(unOp->operand.get(), bp, "");
            std::string outWire = targetWire.empty() ? genTempWire(bp, 0) : targetWire;

            std::string opStr = unOp->op;
            std::transform(opStr.begin(), opStr.end(), opStr.begin(), ::toupper);

            if (opStr == "NOT")
            {
                bp.addComponent(genCompName("not"), 
                    std::make_unique<Pulse::Parser::NotGateInstance>(operandWire, outWire));
            }

            return outWire;
        }

        return "";
    }

    bitWidth_t BlueprintGenerator::evaluateStaticInteger(const ASTNode* node)
    {
        if (auto intLit = dynamic_cast<const IntegerLiteralExpr*>(node))
        {
            return static_cast<bitWidth_t>(intLit->value);
        }
        return 0;
    }
} // namespace Pulse::Parser::VHDL
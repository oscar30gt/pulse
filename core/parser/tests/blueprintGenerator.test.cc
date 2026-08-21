#include <gtest/gtest.h>
#include "BlueprintGenerator.h"
#include <memory>

using namespace Pulse::Parser::VHDL;
using namespace Pulse::Parser;

TEST(BlueprintGeneratorTest, CreatesPortsAndSignals)
{
    LinkedDesign design;
    
    // 1. Mock Entity
    auto entity = std::make_unique<EntityDeclaration>();
    entity->entityName = "Adder";
    
    auto portIn = std::make_unique<PortDeclaration>();
    portIn->portName = "in_A";
    portIn->isInput = true;
    portIn->width = 8;
    entity->ports.push_back(std::move(portIn));

    // 2. Mock Architecture
    LinkedArchitecture arch;
    arch.architectureName = "rtl";
    arch.targetEntity = entity.get();

    auto sig = std::make_unique<SignalDeclaration>();
    sig->signalName = "carry_out";
    sig->width = 1;
    arch.signals.push_back(sig.get());

    design.entities["Adder"] = entity.get();
    design.architectures.push_back(arch);

    // 3. Generate Blueprint
    BlueprintGenerator generator;
    auto blueprints = generator.generate(design);

    ASSERT_NE(blueprints.find("rtl"), blueprints.end());
    Blueprint* bp = blueprints["rtl"].get();

    // Verify Ports
    ASSERT_EQ(bp->inPorts.size(), 1);
    EXPECT_EQ(bp->inPorts[0], "in_A");

    // Verify Signals (Both ports and internal signals become wires in Blueprint)
    ASSERT_NE(bp->wires.find("in_A"), bp->wires.end());
    EXPECT_EQ(bp->wires["in_A"].width, 8);
    
    ASSERT_NE(bp->wires.find("carry_out"), bp->wires.end());
    EXPECT_EQ(bp->wires["carry_out"].width, 1);
}

TEST(BlueprintGeneratorTest, BuildsExpression)
{
    LinkedDesign design;
    LinkedArchitecture arch;
    arch.architectureName = "logic";
    arch.targetEntity = nullptr; // Unbound for this isolated test

    // Mock Assignment: target <= A AND B
    auto assign = std::make_unique<SignalAssignment>();
    
    auto targetRef = std::make_unique<SignalReference>();
    targetRef->signalName = "result";
    assign->target = std::move(targetRef);

    auto andOp = std::make_unique<BinaryOpExpr<ReturnType::LOGIC>>();
    andOp->op = "AND";
    
    auto leftRef = std::make_unique<SignalReference>();
    leftRef->signalName = "A";
    andOp->left = std::move(leftRef);
    
    auto rightRef = std::make_unique<SignalReference>();
    rightRef->signalName = "B";
    andOp->right = std::move(rightRef);

    assign->value = std::move(andOp);
    arch.assignments.push_back(assign.get());
    design.architectures.push_back(arch);

    // Generate Blueprint
    BlueprintGenerator generator;
    auto blueprints = generator.generate(design);
    Blueprint* bp = blueprints["logic"].get();

    // Verify a component was created for the assignment
    ASSERT_EQ(bp->components.size(), 1);
    
    // Verify the component type is a BinaryGate
    auto it = bp->components.begin();
    ComponentInstance* comp = it->second.get();
    ASSERT_EQ(comp->type, InstanceType::BinaryGate);

    // Downcast to inspect specific instance properties
    auto gate = static_cast<BinaryGateInstance*>(comp);
    EXPECT_EQ(gate->in0, "A");
    EXPECT_EQ(gate->in1, "B");
    EXPECT_EQ(gate->out, "result");
    EXPECT_EQ(gate->op, Pulse::Engine::BinaryOp::AND);
}
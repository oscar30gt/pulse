#include <gtest/gtest.h>
#include "Linker.h"
#include <memory>

using namespace Pulse::Parser::VHDL;

// Helper to create a basic entity for testing
std::unique_ptr<EntityDeclaration> createDummyEntity(const std::string& name)
{
    auto entity = std::make_unique<EntityDeclaration>();
    entity->entityName = name;
    
    auto portA = std::make_unique<PortDeclaration>();
    portA->portName = "A";
    portA->isInput = true;
    portA->width = 1;
    
    auto portB = std::make_unique<PortDeclaration>();
    portB->portName = "B";
    portB->isInput = false;
    portB->width = 1;

    entity->ports.push_back(std::move(portA));
    entity->ports.push_back(std::move(portB));
    
    return entity;
}

TEST(LinkerTest, ResolvesEntities)
{
    RootNode root;
    root.children.push_back(createDummyEntity("MyGate"));

    std::vector<const RootNode*> astRoots = { &root };
    Linker linker;
    LinkedDesign design = linker.link(astRoots);

    // Assert the entity was registered correctly
    ASSERT_EQ(design.entities.size(), 1);
    ASSERT_NE(design.entities.find("MyGate"), design.entities.end());
    EXPECT_EQ(design.entities["MyGate"]->ports.size(), 2);
}

TEST(LinkerTest, ResolvesArchitecture)
{
    RootNode root;
    
    // 1. Add Entity
    root.children.push_back(createDummyEntity("TopLevel"));

    // 2. Add Architecture
    auto arch = std::make_unique<ArchitectureDeclaration>();
    arch->architectureName = "behavioral";
    arch->entityName = "TopLevel";

    auto sig = std::make_unique<SignalDeclaration>();
    sig->signalName = "internal_wire";
    sig->width = 1;
    arch->signals.push_back(std::move(sig));

    root.children.push_back(std::move(arch));

    std::vector<const RootNode*> astRoots = { &root };
    Linker linker;
    LinkedDesign design = linker.link(astRoots);

    // Assert the architecture is bound to the entity and signals are populated
    ASSERT_EQ(design.architectures.size(), 1);
    EXPECT_EQ(design.architectures[0].architectureName, "behavioral");
    
    ASSERT_NE(design.architectures[0].targetEntity, nullptr);
    EXPECT_EQ(design.architectures[0].targetEntity->entityName, "TopLevel");
    EXPECT_EQ(design.architectures[0].signals.size(), 1);
}
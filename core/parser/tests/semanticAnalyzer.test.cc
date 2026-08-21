#include <gtest/gtest.h>
#include <memory>
#include <stdexcept>
#include "SemanticAnalyzer.h"

namespace Pulse::Parser::VHDL::Tests
{
    class SemanticAnalyzerTest : public ::testing::Test 
    {
    protected:
        SemanticAnalyzer analyzer;
        RootNode root;

        void SetUp() override 
        {
            // Reset root for each test to ensure a clean AST
            root.children.clear();
        }

        // Helper to add a basic entity to the root node
        EntityDeclaration* addBasicEntity(const std::string& name) 
        {
            auto entity = std::make_unique<EntityDeclaration>();
            entity->entityName = name;
            EntityDeclaration* ptr = entity.get();
            root.children.push_back(std::move(entity));
            return ptr;
        }

        // Helper to add a basic architecture to the root node
        ArchitectureDeclaration* addBasicArchitecture(const std::string& archName, const std::string& entityName) 
        {
            auto arch = std::make_unique<ArchitectureDeclaration>();
            arch->architectureName = archName;
            arch->entityName = entityName;
            ArchitectureDeclaration* ptr = arch.get();
            root.children.push_back(std::move(arch));
            return ptr;
        }
    };

    TEST_F(SemanticAnalyzerTest, AcceptsValidEntityAndArchitecture) 
    {
        addBasicEntity("AndGate");
        addBasicArchitecture("Behavioral", "AndGate");

        // Should not throw any semantic errors
        EXPECT_NO_THROW(analyzer.analyze(root));
    }

    TEST_F(SemanticAnalyzerTest, RejectsDuplicateEntities) 
    {
        addBasicEntity("OrGate");
        addBasicEntity("OrGate"); // Duplicate

        EXPECT_THROW({
            try {
                analyzer.analyze(root);
            } catch (const std::runtime_error& e) {
                EXPECT_NE(std::string(e.what()).find("Duplicate entity declaration"), std::string::npos);
                throw;
            }
        }, std::runtime_error);
    }

    TEST_F(SemanticAnalyzerTest, RejectsArchitectureWithUnknownEntity) 
    {
        addBasicArchitecture("Behavioral", "UnknownGate"); // Entity not declared

        EXPECT_THROW({
            try {
                analyzer.analyze(root);
            } catch (const std::runtime_error& e) {
                EXPECT_NE(std::string(e.what()).find("references unknown entity"), std::string::npos);
                throw;
            }
        }, std::runtime_error);
    }

    TEST_F(SemanticAnalyzerTest, RejectsSignalShadowingPort) 
    {
        auto entity = addBasicEntity("ALU");
        
        auto port = std::make_unique<PortDeclaration>();
        port->portName = "clk";
        port->width = 1;
        port->isInput = true;
        entity->ports.push_back(std::move(port));

        auto arch = addBasicArchitecture("Behavioral", "ALU");
        
        auto sig = std::make_unique<SignalDeclaration>();
        sig->signalName = "clk"; // Shadows port
        sig->width = 1;
        arch->signals.push_back(std::move(sig));

        EXPECT_THROW({
            try {
                analyzer.analyze(root);
            } catch (const std::runtime_error& e) {
                EXPECT_NE(std::string(e.what()).find("shadows an existing port"), std::string::npos);
                throw;
            }
        }, std::runtime_error);
    }

    TEST_F(SemanticAnalyzerTest, RejectsAssignmentToInputPort) 
    {
        auto entity = addBasicEntity("FlipFlop");
        
        auto port = std::make_unique<PortDeclaration>();
        port->portName = "d_in";
        port->width = 1;
        port->isInput = true;
        entity->ports.push_back(std::move(port));

        auto arch = addBasicArchitecture("Behavioral", "FlipFlop");

        auto assignment = std::make_unique<SignalAssignment>();
        auto targetRef = std::make_unique<SignalReference>();
        targetRef->signalName = "d_in";
        assignment->target = std::move(targetRef);
        
        auto valueExpr = std::make_unique<LogicLiteralExpr>();
        valueExpr->width = 1;
        assignment->value = std::move(valueExpr);
        
        arch->assignments.push_back(std::move(assignment));

        EXPECT_THROW({
            try {
                analyzer.analyze(root);
            } catch (const std::runtime_error& e) {
                EXPECT_NE(std::string(e.what()).find("Cannot assign to input port"), std::string::npos);
                throw;
            }
        }, std::runtime_error);
    }

    TEST_F(SemanticAnalyzerTest, RejectsWidthMismatchInAssignment) 
    {
        auto entity = addBasicEntity("Adder");
        
        auto port = std::make_unique<PortDeclaration>();
        port->portName = "sum";
        port->width = 8; // 8-bit target
        port->isInput = false;
        entity->ports.push_back(std::move(port));

        auto arch = addBasicArchitecture("RTL", "Adder");

        auto assignment = std::make_unique<SignalAssignment>();
        auto targetRef = std::make_unique<SignalReference>();
        targetRef->signalName = "sum";
        assignment->target = std::move(targetRef);
        
        auto valueExpr = std::make_unique<LogicLiteralExpr>();
        valueExpr->width = 4; // 4-bit value assigned to 8-bit target
        assignment->value = std::move(valueExpr);
        
        arch->assignments.push_back(std::move(assignment));

        EXPECT_THROW({
            try {
                analyzer.analyze(root);
            } catch (const std::runtime_error& e) {
                EXPECT_NE(std::string(e.what()).find("Width mismatch in assignment"), std::string::npos);
                throw;
            }
        }, std::runtime_error);
    }
} // namespace Pulse::Parser::VHDL::Tests
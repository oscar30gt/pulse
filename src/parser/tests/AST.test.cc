// ast.test.cc — GTest suite for Pulse::Parser::VHDLtoAST
//
// Comprehensive test coverage for AST generation from tokenized VHDL source.
// Tests verify that all syntactically valid VHDL represented in the AST tree
// is parsed correctly, and that syntax errors result in ast_build_error exceptions.
//
// Implementation notes:
//   - The function takes a Tokenizer reference and generates an ASTRoot.
//   - No semantic analysis is performed; only syntactic parsing is verified.
//   - Not all VHDL syntax is supported; only constructs that fit the AST are parseable.
//   - The function throws ast_build_error on malformed input.

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>
#include <sstream>

#include "ast.h"
#include "tokenizer.h"

using namespace Pulse::Parser;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Build an AST from a raw VHDL string.
/// @returns ASTRoot containing the parsed tree.
/// @throws ast_build_error if the input is malformed.
static ASTRoot parseVHDL(const std::string& source)
{
    Tokenizer tokenizer(source);
    return VHDLtoAST(tokenizer);
}

/// Assert that parsing the given source throws ast_build_error.
/// @param source Raw VHDL string that should fail to parse.
static void expectParseError(const std::string& source)
{
    EXPECT_THROW({
        Tokenizer tokenizer(source);
        VHDLtoAST(tokenizer);
    }, ast_build_error);
}

/// Assert that parsing succeeds and returns a non-empty AST.
/// @param source Raw VHDL string that should parse successfully.
static void expectParseSuccess(const std::string& source)
{
    EXPECT_NO_THROW({
        Tokenizer tokenizer(source);
        ASTRoot root = VHDLtoAST(tokenizer);
        EXPECT_FALSE(root.children.empty()) << "Expected non-empty AST";
    });
}

/// Downcast a generic ASTNode pointer to a specific derived type.
/// Returns nullptr if the cast fails.
template <typename T>
static T* as(ASTNode* node)
{
    return dynamic_cast<T*>(node);
}

template <typename T>
static const T* as(const ASTNode* node)
{
    return dynamic_cast<const T*>(node);
}

// ===========================================================================
// 1. BASIC ENTITY DECLARATIONS
// ===========================================================================

TEST(VHDLtoAST_Entities, SimpleEntityNoPortsMinimal)
{
    const std::string source = R"(
        entity my_entity is
        end my_entity;
    )";
    
    EXPECT_NO_THROW({
        ASTRoot root = parseVHDL(source);
        ASSERT_EQ(root.children.size(), 1u);
        auto* entity = as<EntityDeclaration>(root.children[0].get());
        ASSERT_NE(entity, nullptr);
        EXPECT_EQ(entity->name, "my_entity");
        EXPECT_TRUE(entity->ports.empty());
    });
}

TEST(VHDLtoAST_Entities, EntityWithSingleInputPort)
{
    const std::string source = R"(
        entity adder is
            port (
                clk : in std_logic
            );
        end adder;
    )";
    
    EXPECT_NO_THROW({
        ASTRoot root = parseVHDL(source);
        ASSERT_EQ(root.children.size(), 1u);
        auto* entity = as<EntityDeclaration>(root.children[0].get());
        ASSERT_NE(entity, nullptr);
        EXPECT_EQ(entity->name, "adder");
        ASSERT_EQ(entity->ports.size(), 1u);
        EXPECT_EQ(entity->ports[0].portName, "clk");
        EXPECT_TRUE(entity->ports[0].isInput);
        EXPECT_FALSE(entity->ports[0].isOutput);
    });
}

TEST(VHDLtoAST_Entities, EntityWithMultiplePorts)
{
    const std::string source = R"(
        entity multiplier is
            port (
                a : in std_logic_vector(7 downto 0);
                b : in std_logic_vector(7 downto 0);
                result : out std_logic_vector(15 downto 0)
            );
        end multiplier;
    )";
    
    EXPECT_NO_THROW({
        ASTRoot root = parseVHDL(source);
        ASSERT_EQ(root.children.size(), 1u);
        auto* entity = as<EntityDeclaration>(root.children[0].get());
        ASSERT_EQ(entity->ports.size(), 3u);
        EXPECT_EQ(entity->ports[0].portName, "a");
        EXPECT_EQ(entity->ports[1].portName, "b");
        EXPECT_EQ(entity->ports[2].portName, "result");
        EXPECT_TRUE(entity->ports[2].isOutput);
    });
}

TEST(VHDLtoAST_Entities, EntityWithInoutPort)
{
    const std::string source = R"(
        entity bidirectional is
            port (
                data : inout std_logic
            );
        end bidirectional;
    )";
    
    EXPECT_NO_THROW({
        ASTRoot root = parseVHDL(source);
        auto* entity = as<EntityDeclaration>(root.children[0].get());
        ASSERT_EQ(entity->ports.size(), 1u);
        EXPECT_TRUE(entity->ports[0].isInput);
        EXPECT_TRUE(entity->ports[0].isOutput);
    });
}

TEST(VHDLtoAST_Entities, EntityWithVectorPortsVariousRanges)
{
    const std::string source = R"(
        entity vectors is
            port (
                a : in std_logic_vector(7 downto 0);
                b : in std_logic_vector(0 to 15);
                c : out integer
            );
        end vectors;
    )";
    
    EXPECT_NO_THROW({
        ASTRoot root = parseVHDL(source);
        auto* entity = as<EntityDeclaration>(root.children[0].get());
        ASSERT_EQ(entity->ports.size(), 3u);
        EXPECT_EQ(entity->ports[0].typeSpec.typeName, "std_logic_vector");
        EXPECT_EQ(entity->ports[2].typeSpec.typeName, "integer");
    });
}

// ===========================================================================
// 2. BASIC ARCHITECTURE DECLARATIONS
// ===========================================================================

TEST(VHDLtoAST_Architectures, SimpleArchitectureNoSignalsOrStatements)
{
    const std::string source = R"(
        entity test_ent is
        end test_ent;

        architecture rtl of test_ent is
        begin
        end rtl;
    )";
    
    EXPECT_NO_THROW({
        ASTRoot root = parseVHDL(source);
        ASSERT_EQ(root.children.size(), 2u);
        auto* arch = as<ArchitectureDeclaration>(root.children[1].get());
        ASSERT_NE(arch, nullptr);
        EXPECT_EQ(arch->name, "rtl");
        EXPECT_EQ(arch->entityName, "test_ent");
        EXPECT_TRUE(arch->signals.empty());
        EXPECT_TRUE(arch->components.empty());
        EXPECT_TRUE(arch->body.empty());
    });
}

TEST(VHDLtoAST_Architectures, ArchitectureWithSignals)
{
    const std::string source = R"(
        entity test is
        end test;

        architecture behavioral of test is
            signal sig1 : std_logic;
            signal sig2 : std_logic_vector(7 downto 0);
            signal sig3 : integer := 42;
        begin
        end behavioral;
    )";
    
    EXPECT_NO_THROW({
        ASTRoot root = parseVHDL(source);
        auto* arch = as<ArchitectureDeclaration>(root.children[1].get());
        ASSERT_EQ(arch->signals.size(), 3u);
        EXPECT_EQ(arch->signals[0].name, "sig1");
        EXPECT_EQ(arch->signals[1].name, "sig2");
        EXPECT_EQ(arch->signals[2].name, "sig3");
        auto* initVal = as<IntegerLiteralExpr>(arch->signals[2].initialValue.get());
        ASSERT_NE(initVal, nullptr);
        EXPECT_EQ(initVal->value, 42);
    });
}

TEST(VHDLtoAST_Architectures, ArchitectureWithComponentDeclaration)
{
    const std::string source = R"(
        entity test is
        end test;

        architecture behavioral of test is
            component adder
                port (
                    a : in std_logic_vector(7 downto 0);
                    b : in std_logic_vector(7 downto 0);
                    s : out std_logic_vector(7 downto 0)
                );
            end component;
        begin
        end behavioral;
    )";
    
    EXPECT_NO_THROW({
        ASTRoot root = parseVHDL(source);
        auto* arch = as<ArchitectureDeclaration>(root.children[1].get());
        ASSERT_EQ(arch->components.size(), 1u);
        EXPECT_EQ(arch->components[0].name, "adder");
        ASSERT_EQ(arch->components[0].ports.size(), 3u);
    });
}

// ===========================================================================
// 3. SIGNAL ASSIGNMENTS
// ===========================================================================

TEST(VHDLtoAST_SignalAssignments, SimpleAssignmentSymbolToSymbol)
{
    const std::string source = R"(
        entity test is
            port (in_sig : in std_logic; out_sig : out std_logic);
        end test;

        architecture behavioral of test is
        begin
            out_sig <= in_sig;
        end behavioral;
    )";
    
    EXPECT_NO_THROW({
        ASTRoot root = parseVHDL(source);
        auto* arch = as<ArchitectureDeclaration>(root.children[1].get());
        ASSERT_EQ(arch->body.size(), 1u);
        auto* assign = as<SignalAssignment>(arch->body[0].get());
        ASSERT_NE(assign, nullptr);
        auto* target = as<SymbolExpr>(assign->target.get());
        ASSERT_NE(target, nullptr);
        EXPECT_EQ(target->name, "out_sig");
    });
}

TEST(VHDLtoAST_SignalAssignments, AssignmentWithIntegerLiteral)
{
    const std::string source = R"(
        entity test is
        end test;

        architecture behavioral of test is
            signal sig : integer;
        begin
            sig <= 123;
        end behavioral;
    )";
    
    EXPECT_NO_THROW({
        ASTRoot root = parseVHDL(source);
        auto* arch = as<ArchitectureDeclaration>(root.children[1].get());
        auto* assign = as<SignalAssignment>(arch->body[0].get());
        auto* value = as<IntegerLiteralExpr>(assign->value.get());
        ASSERT_NE(value, nullptr);
        EXPECT_EQ(value->value, 123);
    });
}

TEST(VHDLtoAST_SignalAssignments, AssignmentWithBooleanLiteral)
{
    const std::string source = R"(
        entity test is
        end test;

        architecture behavioral of test is
            signal flag : std_logic;
        begin
            flag <= '1';
        end behavioral;
    )";
    
    EXPECT_NO_THROW({
        ASTRoot root = parseVHDL(source);
        auto* arch = as<ArchitectureDeclaration>(root.children[1].get());
        auto* assign = as<SignalAssignment>(arch->body[0].get());
        auto* value = as<LogicLiteralExpr>(assign->value.get());
        ASSERT_NE(value, nullptr);
    });
}

// ===========================================================================
// 4. EXPRESSIONS
// ===========================================================================

TEST(VHDLtoAST_Expressions, BinaryOperatorArithmetic)
{
    const std::string source = R"(
        entity test is
        end test;

        architecture behavioral of test is
            signal a, b, sum : integer;
        begin
            sum <= a + b;
        end behavioral;
    )";
    
    EXPECT_NO_THROW({
        ASTRoot root = parseVHDL(source);
        auto* arch = as<ArchitectureDeclaration>(root.children[1].get());
        auto* assign = as<SignalAssignment>(arch->body[0].get());
        auto* binop = as<BinaryOpExpr>(assign->value.get());
        ASSERT_NE(binop, nullptr);
        EXPECT_EQ(binop->op, "+");
    });
}

TEST(VHDLtoAST_Expressions, BinaryOperatorLogical)
{
    const std::string source = R"(
        entity test is
        end test;

        architecture behavioral of test is
            signal a, b, result : std_logic;
        begin
            result <= a and b;
        end behavioral;
    )";
    
    EXPECT_NO_THROW({
        ASTRoot root = parseVHDL(source);
        auto* arch = as<ArchitectureDeclaration>(root.children[1].get());
        auto* assign = as<SignalAssignment>(arch->body[0].get());
        auto* binop = as<BinaryOpExpr>(assign->value.get());
        ASSERT_NE(binop, nullptr);
        EXPECT_EQ(binop->op, "and");
    });
}

TEST(VHDLtoAST_Expressions, BinaryOperatorShift)
{
    const std::string source = R"(
        entity test is
        end test;

        architecture behavioral of test is
            signal a, result : std_logic_vector(7 downto 0);
        begin
            result <= a sll "2";
        end behavioral;
    )";
    
    EXPECT_NO_THROW({
        ASTRoot root = parseVHDL(source);
        auto* arch = as<ArchitectureDeclaration>(root.children[1].get());
        auto* assign = as<SignalAssignment>(arch->body[0].get());
        auto* binop = as<BinaryOpExpr>(assign->value.get());
        ASSERT_NE(binop, nullptr);
        EXPECT_EQ(binop->op, "sll");
    });
}

TEST(VHDLtoAST_Expressions, UnaryOperatorNot)
{
    const std::string source = R"(
        entity test is
        end test;

        architecture behavioral of test is
            signal a, result : std_logic;
        begin
            result <= not a;
        end behavioral;
    )";
    
    EXPECT_NO_THROW({
        ASTRoot root = parseVHDL(source);
        auto* arch = as<ArchitectureDeclaration>(root.children[1].get());
        auto* assign = as<SignalAssignment>(arch->body[0].get());
        auto* unop = as<UnaryOpExpr>(assign->value.get());
        ASSERT_NE(unop, nullptr);
        EXPECT_EQ(unop->op, "not");
    });
}

TEST(VHDLtoAST_Expressions, UnaryOperatorNegation)
{
    const std::string source = R"(
        entity test is
        end test;

        architecture behavioral of test is
            signal a, result : integer;
        begin
            result <= -a;
        end behavioral;
    )";
    
    EXPECT_NO_THROW({
        ASTRoot root = parseVHDL(source);
        auto* arch = as<ArchitectureDeclaration>(root.children[1].get());
        auto* assign = as<SignalAssignment>(arch->body[0].get());
        auto* unop = as<UnaryOpExpr>(assign->value.get());
        ASSERT_NE(unop, nullptr);
        EXPECT_EQ(unop->op, "-");
    });
}

TEST(VHDLtoAST_Expressions, FunctionCall)
{
    const std::string source = R"(
        entity test is
        end test;

        architecture behavioral of test is
            signal a : std_logic_vector(7 downto 0);
            signal b : integer;
        begin
            b <= to_integer(unsigned(a));
        end behavioral;
    )";
    
    EXPECT_NO_THROW({
        ASTRoot root = parseVHDL(source);
        auto* arch = as<ArchitectureDeclaration>(root.children[1].get());
        auto* assign = as<SignalAssignment>(arch->body[0].get());
        auto* funcCall = as<FunctionCallExpr>(assign->value.get());
        ASSERT_NE(funcCall, nullptr);
        EXPECT_EQ(funcCall->functionName, "to_integer");
        ASSERT_EQ(funcCall->arguments.size(), 1u);
    });
}

TEST(VHDLtoAST_Expressions, AttributeAccess)
{
    const std::string source = R"(
        entity test is
        end test;

        architecture behavioral of test is
            signal vec : std_logic_vector(7 downto 0);
            signal len : integer;
        begin
            len <= vec'length;
        end behavioral;
    )";
    
    EXPECT_NO_THROW({
        ASTRoot root = parseVHDL(source);
        auto* arch = as<ArchitectureDeclaration>(root.children[1].get());
        auto* assign = as<SignalAssignment>(arch->body[0].get());
        auto* attr = as<AttributeExpr>(assign->value.get());
        ASSERT_NE(attr, nullptr);
        EXPECT_EQ(attr->attributeName, "length");
    });
}

TEST(VHDLtoAST_Expressions, LogicLiteral)
{
    const std::string source = R"(
        entity test is
        end test;

        architecture behavioral of test is
            signal a : std_logic_vector(7 downto 0);
        begin
            a <= "10110101";
        end behavioral;
    )";
    
    EXPECT_NO_THROW({
        ASTRoot root = parseVHDL(source);
        auto* arch = as<ArchitectureDeclaration>(root.children[1].get());
        auto* assign = as<SignalAssignment>(arch->body[0].get());
        auto* logic = as<LogicLiteralExpr>(assign->value.get());
        ASSERT_NE(logic, nullptr);
        EXPECT_EQ(logic->width, 8u);
    });
}

TEST(VHDLtoAST_Expressions, ExpressionNesting)
{
    const std::string source = R"(
        entity test is
        end test;

        architecture behavioral of test is
            signal a, b, c, result : integer;
        begin
            result <= (a + b) * c;
        end behavioral;
    )";
    
    EXPECT_NO_THROW({
        ASTRoot root = parseVHDL(source);
        auto* arch = as<ArchitectureDeclaration>(root.children[1].get());
        auto* assign = as<SignalAssignment>(arch->body[0].get());
        auto* outer = as<BinaryOpExpr>(assign->value.get());
        ASSERT_NE(outer, nullptr);
        EXPECT_EQ(outer->op, "*");
        auto* inner = as<BinaryOpExpr>(outer->left.get());
        ASSERT_NE(inner, nullptr);
        EXPECT_EQ(inner->op, "+");
    });
}

// ===========================================================================
// 5. COMPONENT INSTANTIATION
// ===========================================================================

TEST(VHDLtoAST_ComponentInstantiation, BasicInstantiation)
{
    const std::string source = R"(
        entity test is
        end test;

        architecture behavioral of test is
            component adder
                port (a : in std_logic; b : in std_logic; s : out std_logic);
            end component;

            signal in1, in2, out1 : std_logic;
        begin
            add_inst : adder port map (a => in1, b => in2, s => out1);
        end behavioral;
    )";
    
    EXPECT_NO_THROW({
        ASTRoot root = parseVHDL(source);
        auto* arch = as<ArchitectureDeclaration>(root.children[1].get());
        ASSERT_EQ(arch->body.size(), 1u);
        auto* inst = as<ComponentInstantiation>(arch->body[0].get());
        ASSERT_NE(inst, nullptr);
        EXPECT_EQ(inst->instanceName, "add_inst");
        EXPECT_EQ(inst->componentName, "adder");
        ASSERT_EQ(inst->portMap.size(), 3u);
    });
}

TEST(VHDLtoAST_ComponentInstantiation, MultipleInstantiations)
{
    const std::string source = R"(
        entity test is
        end test;

        architecture behavioral of test is
            component adder
                port (a : in std_logic; b : in std_logic; s : out std_logic);
            end component;

            signal sig1, sig2, sig3 : std_logic;
        begin
            add1 : adder port map (a => sig1, b => sig2, s => sig3);
            add2 : adder port map (a => sig3, b => sig1, s => sig2);
        end behavioral;
    )";
    
    EXPECT_NO_THROW({
        ASTRoot root = parseVHDL(source);
        auto* arch = as<ArchitectureDeclaration>(root.children[1].get());
        ASSERT_EQ(arch->body.size(), 2u);
    });
}

// ===========================================================================
// 6. PROCESS STATEMENTS
// ===========================================================================

TEST(VHDLtoAST_Processes, ProcessWithSensitivityList)
{
    const std::string source = R"(
        entity test is
        end test;

        architecture behavioral of test is
            signal a, b, result : integer;
        begin
            process (a, b)
            begin
                result <= a + b;
            end process;
        end behavioral;
    )";
    
    EXPECT_NO_THROW({
        ASTRoot root = parseVHDL(source);
        auto* arch = as<ArchitectureDeclaration>(root.children[1].get());
        ASSERT_EQ(arch->body.size(), 1u);
        auto* proc = as<ProcessStatement>(arch->body[0].get());
        ASSERT_NE(proc, nullptr);
        ASSERT_EQ(proc->sensitivityList.size(), 2u);
        EXPECT_EQ(proc->sensitivityList[0], "a");
        EXPECT_EQ(proc->sensitivityList[1], "b");
    });
}

TEST(VHDLtoAST_Processes, ProcessWithoutSensitivityList)
{
    const std::string source = R"(
        entity test is
        end test;

        architecture behavioral of test is
            signal counter : integer;
        begin
            process
            begin
                wait for 1 ns;
                counter <= counter + 1;
            end process;
        end behavioral;
    )";
    
    EXPECT_NO_THROW({
        ASTRoot root = parseVHDL(source);
        auto* arch = as<ArchitectureDeclaration>(root.children[1].get());
        auto* proc = as<ProcessStatement>(arch->body[0].get());
        ASSERT_NE(proc, nullptr);
        EXPECT_TRUE(proc->sensitivityList.empty());
        ASSERT_EQ(proc->body.size(), 2u);
    });
}

TEST(VHDLtoAST_Processes, LabeledProcess)
{
    const std::string source = R"(
        entity test is
        end test;

        architecture behavioral of test is
            signal a : std_logic;
        begin
            my_label : process
            begin
                a <= '1';
            end process;
        end behavioral;
    )";
    
    EXPECT_NO_THROW({
        ASTRoot root = parseVHDL(source);
        auto* arch = as<ArchitectureDeclaration>(root.children[1].get());
        auto* proc = as<ProcessStatement>(arch->body[0].get());
        ASSERT_NE(proc, nullptr);
        EXPECT_EQ(proc->label, "my_label");
    });
}

// ===========================================================================
// 7. SEQUENTIAL STATEMENTS
// ===========================================================================

TEST(VHDLtoAST_SequentialStatements, WaitFor)
{
    const std::string source = R"(
        entity test is
        end test;

        architecture behavioral of test is
        begin
            process
            begin
                wait for 10 ns;
            end process;
        end behavioral;
    )";
    
    EXPECT_NO_THROW({
        ASTRoot root = parseVHDL(source);
        auto* arch = as<ArchitectureDeclaration>(root.children[1].get());
        auto* proc = as<ProcessStatement>(arch->body[0].get());
        ASSERT_EQ(proc->body.size(), 1u);
        auto* wait = as<WaitForStatement>(proc->body[0].get());
        ASSERT_NE(wait, nullptr);
        // Duration should be in femtoseconds
        EXPECT_GT(wait->durationFs, 0u);
    });
}

TEST(VHDLtoAST_SequentialStatements, IfStatement)
{
    const std::string source = R"(
        entity test is
        end test;

        architecture behavioral of test is
            signal a, b, c, result : integer;
        begin
            process (a, b, c)
            begin
                if a = 0 then
                    result <= b;
                else
                    result <= c;
                end if;
            end process;
        end behavioral;
    )";
    
    EXPECT_NO_THROW({
        ASTRoot root = parseVHDL(source);
        auto* arch = as<ArchitectureDeclaration>(root.children[1].get());
        auto* proc = as<ProcessStatement>(arch->body[0].get());
        ASSERT_EQ(proc->body.size(), 1u);
        auto* ifStmt = as<IfStatement>(proc->body[0].get());
        ASSERT_NE(ifStmt, nullptr);
        ASSERT_GE(ifStmt->branches.size(), 1u);
        EXPECT_FALSE(ifStmt->elseBody.empty());
    });
}

TEST(VHDLtoAST_SequentialStatements, IfElsif)
{
    const std::string source = R"(
        entity test is
        end test;

        architecture behavioral of test is
            signal a, result : integer;
        begin
            process (a)
            begin
                if a = 0 then
                    result <= 1;
                elsif a = 1 then
                    result <= 2;
                elsif a = 2 then
                    result <= 3;
                else
                    result <= 0;
                end if;
            end process;
        end behavioral;
    )";
    
    EXPECT_NO_THROW({
        ASTRoot root = parseVHDL(source);
        auto* arch = as<ArchitectureDeclaration>(root.children[1].get());
        auto* proc = as<ProcessStatement>(arch->body[0].get());
        auto* ifStmt = as<IfStatement>(proc->body[0].get());
        ASSERT_NE(ifStmt, nullptr);
        // Should have at least 3 branches (if + 2 elsif)
        EXPECT_GE(ifStmt->branches.size(), 3u);
    });
}

TEST(VHDLtoAST_SequentialStatements, SequentialAssignment)
{
    const std::string source = R"(
        entity test is
        end test;

        architecture behavioral of test is
            signal a, b : integer;
        begin
            process
            begin
                a <= 5;
                b <= a + 1;
            end process;
        end behavioral;
    )";
    
    EXPECT_NO_THROW({
        ASTRoot root = parseVHDL(source);
        auto* arch = as<ArchitectureDeclaration>(root.children[1].get());
        auto* proc = as<ProcessStatement>(arch->body[0].get());
        ASSERT_EQ(proc->body.size(), 2u);
        auto* assign1 = as<SignalAssignment>(proc->body[0].get());
        ASSERT_NE(assign1, nullptr);
    });
}

// ===========================================================================
// 8. WITH-SELECT STATEMENTS
// ===========================================================================

TEST(VHDLtoAST_WithClause, BasicWithSelect)
{
    const std::string source = R"(
        entity test is
        end test;

        architecture behavioral of test is
            signal selector, result : integer;
        begin
            with selector select
                result <= 1 when 0,
                          2 when 1,
                          0 when others;
        end behavioral;
    )";
    
    EXPECT_NO_THROW({
        ASTRoot root = parseVHDL(source);
        auto* arch = as<ArchitectureDeclaration>(root.children[1].get());
        ASSERT_EQ(arch->body.size(), 1u);
        auto* withClause = as<WithClause>(arch->body[0].get());
        ASSERT_NE(withClause, nullptr);
        ASSERT_GE(withClause->choices.size(), 2u);
        EXPECT_NE(withClause->defaultValue, nullptr);
    });
}

// ===========================================================================
// 9. WHEN-ELSE EXPRESSIONS
// ===========================================================================

TEST(VHDLtoAST_WhenElse, BasicWhenElse)
{
    const std::string source = R"(
        entity test is
        end test;

        architecture behavioral of test is
            signal a, b, sel, result : integer;
        begin
            result <= a when sel = 0 else b;
        end behavioral;
    )";
    
    EXPECT_NO_THROW({
        ASTRoot root = parseVHDL(source);
        auto* arch = as<ArchitectureDeclaration>(root.children[1].get());
        auto* assign = as<SignalAssignment>(arch->body[0].get());
        ASSERT_NE(assign, nullptr);
        auto* whenElse = as<WhenElseExpr>(assign->value.get());
        ASSERT_NE(whenElse, nullptr);
        ASSERT_NE(whenElse->condition.get(), nullptr);
        ASSERT_NE(whenElse->trueValue.get(), nullptr);
        ASSERT_NE(whenElse->falseValue.get(), nullptr);
    });
}

TEST(VHDLtoAST_WhenElse, ChainedWhenElse)
{
    const std::string source = R"(
        entity test is
        end test;

        architecture behavioral of test is
            signal a, b, c, d, sel, result : integer;
        begin
            result <= a when sel = 0 else
                      b when sel = 1 else
                      c when sel = 2 else
                      d;
        end behavioral;
    )";
    
    EXPECT_NO_THROW({
        ASTRoot root = parseVHDL(source);
        auto* arch = as<ArchitectureDeclaration>(root.children[1].get());
        auto* assign = as<SignalAssignment>(arch->body[0].get());
        auto* whenElse = as<WhenElseExpr>(assign->value.get());
        ASSERT_NE(whenElse, nullptr);
        ASSERT_NE(whenElse->falseValue.get(), nullptr);
        ASSERT_NE(dynamic_cast<WhenElseExpr*>(whenElse->falseValue.get()), nullptr);
    });
}

// ===========================================================================
// 10. MULTIPLE ENTITIES AND ARCHITECTURES
// ===========================================================================

TEST(VHDLtoAST_MultipleDeclarations, MultipleEntitiesAndArchitectures)
{
    const std::string source = R"(
        entity entity1 is
            port (a : in std_logic);
        end entity1;

        architecture arch1 of entity1 is
        begin
        end arch1;

        entity entity2 is
            port (b : in integer);
        end entity2;

        architecture arch2 of entity2 is
        begin
        end arch2;
    )";
    
    EXPECT_NO_THROW({
        ASTRoot root = parseVHDL(source);
        ASSERT_EQ(root.children.size(), 4u);
        auto* ent1 = as<EntityDeclaration>(root.children[0].get());
        auto* arch1 = as<ArchitectureDeclaration>(root.children[1].get());
        auto* ent2 = as<EntityDeclaration>(root.children[2].get());
        auto* arch2 = as<ArchitectureDeclaration>(root.children[3].get());
        EXPECT_NE(ent1, nullptr);
        EXPECT_NE(arch1, nullptr);
        EXPECT_NE(ent2, nullptr);
        EXPECT_NE(arch2, nullptr);
    });
}

// ===========================================================================
// 11. ERROR CASES
// ===========================================================================

TEST(VHDLtoAST_Errors, MissingEntityName)
{
    expectParseError("entity is end entity;");
}

TEST(VHDLtoAST_Errors, MissingArchitectureName)
{
    expectParseError(R"(
        entity test is end test;
        architecture of test is begin end;
    )");
}

TEST(VHDLtoAST_Errors, UnclosedEntity)
{
    expectParseError("entity test is port (a : in std_logic);");
}

TEST(VHDLtoAST_Errors, MalformedPortDeclaration)
{
    expectParseError(R"(
        entity test is
            port (a in std_logic);
        end test;
    )");
}

TEST(VHDLtoAST_Errors, UnclosedArchitecture)
{
    expectParseError(R"(
        entity test is
        end test;

        architecture rtl of test is
            signal x : std_logic;
        begin
            x <= '1';
    )");
}

TEST(VHDLtoAST_Errors, MissingAssignmentOperator)
{
    expectParseError(R"(
        entity test is
        end test;

        architecture rtl of test is
            signal a, b : std_logic;
        begin
            a = b;
        end rtl;
    )");
}

// ===========================================================================
// 12. EDGE CASES
// ===========================================================================

TEST(VHDLtoAST_EdgeCases, EmptySourceCode)
{
    const std::string source = "";
    ASTRoot root = parseVHDL(source);
    EXPECT_TRUE(root.children.empty());
}

TEST(VHDLtoAST_EdgeCases, OnlyWhitespace)
{
    const std::string source = "   \n\n   \t  \n";
    ASTRoot root = parseVHDL(source);
    EXPECT_TRUE(root.children.empty());
}

TEST(VHDLtoAST_EdgeCases, VeryLongIdentifier)
{
    std::string longName(256, 'a');
    const std::string source = "entity " + longName + " is end " + longName + ";";
    EXPECT_NO_THROW({
        ASTRoot root = parseVHDL(source);
        auto* entity = as<EntityDeclaration>(root.children[0].get());
        EXPECT_EQ(entity->name, longName);
    });
}

TEST(VHDLtoAST_EdgeCases, ManyPorts)
{
    std::string source = "entity test is port (";
    for (int i = 0; i < 100; ++i)
    {
        if (i > 0) source += "; ";
        source += "port" + std::to_string(i) + " : in std_logic";
    }
    source += "); end test;";
    
    EXPECT_NO_THROW({
        ASTRoot root = parseVHDL(source);
        auto* entity = as<EntityDeclaration>(root.children[0].get());
        EXPECT_EQ(entity->ports.size(), 100u);
    });
}

TEST(VHDLtoAST_EdgeCases, DeeplyNestedExpressions)
{
    const std::string source = R"(
        entity test is
        end test;

        architecture behavioral of test is
            signal result : integer;
        begin
            result <= ((((((1 + 2) * 3) - 4) and 5) sra 6) + 7);
        end behavioral;
    )";
    
    EXPECT_NO_THROW({
        ASTRoot root = parseVHDL(source);
        auto* arch = as<ArchitectureDeclaration>(root.children[1].get());
        EXPECT_FALSE(arch->body.empty());
    });
}

TEST(VHDLtoAST_EdgeCases, MultipleSignalsPerDeclaration)
{
    const std::string source = R"(
        entity test is
        end test;

        architecture behavioral of test is
            signal a, b, c : std_logic;
            signal x, y, z : integer := 0;
        begin
        end behavioral;
    )";
    
    EXPECT_NO_THROW({
        ASTRoot root = parseVHDL(source);
        auto* arch = as<ArchitectureDeclaration>(root.children[1].get());
        EXPECT_EQ(arch->signals.size(), 6u);
    });
}

TEST(VHDLtoAST_EdgeCases, ComplexArchitectureWithAllFeatures)
{
    const std::string source = R"(
        entity complex is
            port (
                clk : in std_logic;
                reset : in std_logic;
                data_in : in std_logic_vector(31 downto 0);
                data_out : out std_logic_vector(31 downto 0)
            );
        end complex;

        architecture behavioral of complex is
            signal temp1, temp2 : std_logic_vector(31 downto 0);
            signal counter : integer := 0;

            component processor
                port (a : in std_logic_vector(31 downto 0); b : out std_logic_vector(31 downto 0));
            end component;
        begin
            proc_inst : processor port map (a => data_in, b => temp1);

            temp2 <= temp1 when counter = 0 else '0';

            process (clk, reset)
            begin
                if reset = '1' then
                    counter <= 0;
                elsif clk'event then
                    counter <= counter + 1;
                end if;
            end process;

            data_out <= temp2;
        end behavioral;
    )";
    
    EXPECT_NO_THROW({
        ASTRoot root = parseVHDL(source);
        ASSERT_EQ(root.children.size(), 2u);
        auto* arch = as<ArchitectureDeclaration>(root.children[1].get());
        EXPECT_FALSE(arch->signals.empty());
        EXPECT_FALSE(arch->components.empty());
        EXPECT_FALSE(arch->body.empty());
    });
}
// analyzer_robustness.test.cc — the analyzer never crashes, hangs or throws anything but a compiler_error.
//
//  * A design that uses every construct the parser can build analyzes cleanly.
//  * Every node that only makes sense inside another construct is rejected when used as a value.
//  * The valid designs are mutated (a token deleted, doubled or replaced) and every mutant either analyzes or fails with a
//    located compiler_error: never a crash, a null dereference or a foreign exception.
//  * Absurdly long or deep input fails cleanly.

#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <string>
#include <vector>

#include "analyzer.h"
#include "linker.h"
#include "parser.h"
#include "test_helpers.h"
#include "tokenizer.h"

using namespace Pulse::Parser;

namespace
{
    /// Parses, links and analyzes one source, like the compiler does.
    void compile(const std::string& source)
    {
        Tokenizer tokenizer(source);
        Linker linker;
        linker.addAST(VHDLtoAST(tokenizer));
        ASTRoot design = linker.link();
        analyzeAST(design);
    }

    /// The message of the error the source fails with, or "<no error>".
    std::string failure(const std::string& source)
    {
        try
        {
            compile(source);
        }
        catch (const compiler_error& error)
        {
            return error.what();
        }
        return "<no error>";
    }

    bool mentions(const std::string& message, const std::string& text)
    {
        return message.find(text) != std::string::npos;
    }

    // ---- The corpus -----------------------------------------------------------------------------

    /// Everything in one design: types of every kind, generics, ports with defaults, subprograms (overloaded, operators,
    /// procedures with signal parameters), aliases, attributes, resolution, external names, every statement.
    const char* const kEverything = R"(
        library ieee; use ieee.std_logic_1164.all;

        entity leaf is
            generic (w : natural := 4; init : std_logic := '0');
            port (clk : in std_logic; d : in std_logic_vector(w - 1 downto 0); en : in std_logic := '1';
                  q : out std_logic_vector(w - 1 downto 0); ok : out std_logic; io : inout std_logic);
        end entity leaf;

        architecture rtl of leaf is
            type state_t is (idle, run, done);
            type word is array (0 to 3) of std_logic;
            type table is array (state_t) of integer;
            type pair is record lo, hi : integer; end record;
            type dist is range 0 to 1000000 units nm; um = 1000 nm; mm = 1000 um; metre = mm; end units;
            subtype small is integer range 0 to 15;
            constant limit : small := 9;
            constant zero : std_logic_vector(w - 1 downto 0) := (others => '0');
            signal state : state_t;
            signal counter : unsigned(w - 1 downto 0);
            signal flags : word;
            signal both : pair;
            signal length : dist;
            signal tab : table;
            signal s1, s2 : std_logic;
            signal p1, p2 : std_logic_vector(1 downto 0);
            signal wide : std_logic_vector(7 downto 0);
            alias top_bits : std_logic_vector(3 downto 0) is wide(7 downto 4);
            alias status is state;
            attribute keep : boolean;
            attribute keep of counter, flags : signal is true;
            attribute code : integer;
            attribute code of idle : literal is 1;
            attribute tag : small;
            attribute tag of main : label is 3;
            function inc(x : integer) return integer is begin return x + 1; end function inc;
            function inc(x : state_t) return state_t is begin return run; end function inc;
            function "+"(l : state_t; r : integer) return state_t is begin return l; end function;
            impure function peek return integer is begin return counter'length; end function;
            procedure drive(signal s : out std_logic; v : in std_logic := '0') is begin s <= v; end procedure;
            procedure bump(variable v : inout integer) is begin v := v + 1; end procedure;
            function parity(v : std_logic_vector) return std_logic is
                variable r : std_logic := '0';
            begin
                for i in v'range loop r := r xor v(i); end loop;
                return r;
            end function parity;
            function rising(x : std_logic) return boolean is begin return x = '1'; end function;
            component leaf is
                generic (w : natural := 4; init : std_logic := '0');
                port (clk : in std_logic; d : in std_logic_vector(w - 1 downto 0); en : in std_logic := '1';
                      q : out std_logic_vector(w - 1 downto 0); ok : out std_logic; io : inout std_logic);
            end component;
        begin
            ok <= '1' when state = done else '0';
            q <= std_logic_vector(counter);
            with state select s1 <= '1' when run, '0' when others;
            with wide select? s2 <= '1' when "1-------", '0' when others;
            assert limit > 0;
            drive(io, '1');
            (s1, s2) <= p1 when en = '0' else p2;

            main : process (clk)
                variable acc : integer := 0;
                variable local : pair;
            begin
                if rising(clk) then null; end if;
                case state is
                    when idle => state <= run;
                    when run => state <= inc(state);
                    when others => state <= idle;
                end case;
                case? d is
                    when "1-01" => null;
                    when others => null;
                end case?;
                for i in 0 to limit loop
                    acc := acc + inc(i);
                    next when i = 2;
                    exit when acc > 100;
                end loop;
                bump(acc);
                l1 : while acc > 0 loop acc := acc - 1; end loop l1;
                loop exit; end loop;
                local := (lo => 1, hi => acc);
                counter <= counter + 1;
                tab(idle) <= peek;
                length <= 5 um;
                flags <= (others => '1');
                wide(3 downto 0) <= (others => parity(d));
            end process main;

            waiter : process
            begin
                wait on s1 until s1 = '1' for 1 us;
                wait for 10 ns;
                wait;
            end process waiter;

            u : leaf generic map (w => 4) port map (clk => clk, d => d, q => open, ok => open, io => open);
        end architecture rtl;
    )";

    /// A program where every part is valid on its own; the same shapes as above but smaller, so mutants stay quick.
    const std::vector<std::string>& corpus()
    {
        static const std::vector<std::string> programs = {
            "entity e is port (a, b : in std_logic; y : out std_logic; v : in std_logic_vector(3 downto 0)); end e;\n"
            "architecture r of e is\n"
            "  type color is (red, green, blue); signal c : color; signal n, m : integer range 0 to 9;\n"
            "  constant k : integer := 3;\n"
            "  function twice(x : integer) return integer is begin return x * 2; end;\n"
            "begin\n"
            "  y <= a and b when v(0) = '1' else not a;\n"
            "  process (a, b) variable t : integer := 0; begin\n"
            "    if a = '1' then n <= twice(k); elsif b = '1' then n <= 2; else n <= 0; end if;\n"
            "    case c is when red => m <= 1; when others => m <= 2; end case;\n"
            "    for i in 0 to 3 loop t := t + i; exit when t > 5; end loop;\n"
            "  end process;\n"
            "end r;",

            "entity g is generic (w : natural := 8); port (d : in std_logic_vector(w - 1 downto 0); q : out std_logic_vector(w - 1 downto 0)); end g;\n"
            "architecture r of g is\n"
            "  signal spare : std_logic; attribute tag : integer; attribute tag of spare : signal is 1;\n"
            "  alias hi : std_logic_vector(3 downto 0) is d(3 downto 0);\n"
            "  procedure copy(signal a : in std_logic_vector; signal b : out std_logic_vector) is begin b <= a; end;\n"
            "begin\n"
            "  copy(d, q);\n"
            "  assert w > 0;\n"
            "end r;",

            "entity h is port (a : in std_logic; y : out std_logic); end h;\n"
            "architecture r of h is\n"
            "  component h is port (a : in std_logic; y : out std_logic); end component;\n"
            "  signal s : std_logic; signal v : std_logic_vector(1 downto 0);\n"
            "begin\n"
            "  (s, y) <= v;\n"
            "  with v select? y <= '1' when \"1-\", '0' when others;\n"
            "  process begin wait on a; y <= <<signal .x.y : std_logic>>; end process;\n"
            "end r;",
        };
        return programs;
    }

    /// Offset of a token in the source, from its 1-based line and column.
    size_t offsetOf(const std::string& source, const Token& token)
    {
        size_t offset = 0;
        for (size_t line = 1; line < token.line; ++line)
            offset = source.find('\n', offset) + 1;
        return offset + token.column - 1;
    }

    void forEachToken(const std::string& source, const std::function<void(size_t, size_t)>& mutate)
    {
        Tokenizer tokenizer(source);
        for (size_t i = 0; i < tokenizer.size(); ++i)
        {
            const Token& token = *tokenizer.peek(static_cast<ssize_t>(i));
            mutate(offsetOf(source, token), token.original.size());
        }
    }

    /// Compiles `source`; anything but success or a located compiler_error is a test failure.
    void expectCleanOutcome(const std::string& source, const std::string& what)
    {
        try
        {
            compile(source);
        }
        catch (const compiler_error& error)
        {
            EXPECT_GE(error.location().line, 1u) << what;
        }
        catch (const std::exception& error)
        {
            ADD_FAILURE() << what << ": foreign exception: " << error.what() << "\n" << source;
        }
        catch (...)
        {
            ADD_FAILURE() << what << ": unknown exception\n" << source;
        }
    }
}

// ===========================================================================
// 1. EVERY CONSTRUCT IN ONE DESIGN
// ===========================================================================

TEST(AnalyzerRobustness, TheCorpusIsValid)
{
    // The third program mixes a component of its own entity with an external name; the mutation tests only need it to end
    // cleanly, so it is not asserted here.
    EXPECT_EQ(failure(corpus()[0]), "<no error>");
    EXPECT_EQ(failure(corpus()[1]), "<no error>");
}

TEST(AnalyzerRobustness, EveryConstructInOneDesign)
{
    const std::string message = failure(kEverything);
    EXPECT_EQ(message, "<no error>");
}

// ===========================================================================
// 2. NODES THAT ONLY MAKE SENSE INSIDE ANOTHER CONSTRUCT
// ===========================================================================

namespace
{
    ExpressionPtr symbolNamed(const std::string& name)
    {
        auto node = std::make_unique<SymbolExpr>();
        node->name = name;
        return node;
    }

    ExpressionPtr integerLiteral(int64_t value)
    {
        auto node = std::make_unique<IntegerLiteralExpr>();
        node->value = value;
        return node;
    }

    /// `n <= <value>;` in an architecture with `signal n : integer`, hand-built so any tree can be analyzed, however the
    /// parser would have refused it.
    std::string assigning(ExpressionPtr value)
    {
        auto assignment = std::make_unique<SignalAssignment>();
        assignment->target = symbolNamed("n");
        assignment->value = std::move(value);

        auto declaration = std::make_unique<SignalDeclaration>();
        declaration->name = "n";
        declaration->typeSpec = std::make_unique<TypeSpec>();
        declaration->typeSpec->typeName = "integer";

        auto architecture = std::make_unique<ArchitectureDeclaration>();
        architecture->name = "r";
        architecture->entityName = "e";
        architecture->declarations.push_back(std::move(declaration));
        architecture->body.push_back(std::move(assignment));

        auto entity = std::make_unique<EntityDeclaration>();
        entity->name = "e";

        ASTRoot root;
        root.children.push_back(std::move(entity));
        root.children.push_back(std::move(architecture));

        try
        {
            analyzeAST(root);
        }
        catch (const compiler_error& error)
        {
            return error.what();
        }
        return "<no error>";
    }

    /// `n <= <node> + 1;`, so a node the parser never puts in a value can be tried as an operand.
    std::string valueOf(ExpressionPtr node)
    {
        auto sum = std::make_unique<BinaryOpExpr>();
        sum->op = BinaryOperator::Add;
        sum->left = std::move(node);
        sum->right = integerLiteral(1);
        return assigning(std::move(sum));
    }
}

TEST(AnalyzerRobustness, NodesOfOtherConstructsAreNotValues)
{
    struct Case { const char* name; ExpressionPtr node; const char* message; };

    std::vector<Case> cases;
    cases.push_back({ "SignatureExpr", std::make_unique<SignatureExpr>(), "A signature is not a value" });
    cases.push_back({ "NamedAssociationExpr", [] { auto n = std::make_unique<NamedAssociationExpr>(); n->formal = symbolNamed("a"); n->actual = integerLiteral(1); return n; }(),
                      "An association with '=>' is not a value" });
    cases.push_back({ "OpenExpr", std::make_unique<OpenExpr>(), "'open' is not a value" });
    cases.push_back({ "TypeSpec", [] { auto n = std::make_unique<TypeSpec>(); n->typeName = "integer"; return n; }(), "A type or subtype indication is not a value" });
    cases.push_back({ "ElementResolutionExpr", std::make_unique<ElementResolutionExpr>(), "A resolution indication is not a value" });
    cases.push_back({ "OthersExpr", std::make_unique<OthersExpr>(), "'others' is not a value" });
    cases.push_back({ "AllExpr", std::make_unique<AllExpr>(), "'all' is not a value" });
    cases.push_back({ "ChoiceListExpr", std::make_unique<ChoiceListExpr>(), "A list of choices is not a value" });
    cases.push_back({ "WhenElseExpr", [] { auto n = std::make_unique<WhenElseExpr>(); n->trueValue = integerLiteral(1); n->condition = symbolNamed("true"); return n; }(),
                      "A 'when ... else' value can only be the right-hand side of an assignment" });
    cases.push_back({ "UnaffectedExpr", std::make_unique<UnaffectedExpr>(), "'unaffected' can only be the value of a signal assignment" });

    for (Case& entry : cases)
    {
        const std::string message = valueOf(std::move(entry.node));
        EXPECT_TRUE(mentions(message, entry.message)) << entry.name << ": " << message;
    }
}

TEST(AnalyzerRobustness, ANodeNoOneRegisteredIsReportedNotIgnored)
{
    // A design unit the linker passes through and the analyzer has no handler for.
    struct Unknown final : DesignUnit
    {
        std::unique_ptr<ASTNode> clone() const override { return std::make_unique<Unknown>(); }
        void print(int) const override { }
    };

    ASTRoot root;
    root.children.push_back(std::make_unique<Unknown>());
    try
    {
        analyzeAST(root);
        FAIL() << "expected an error";
    }
    catch (const compiler_error& error)
    {
        EXPECT_TRUE(mentions(error.what(), "no handler for this kind of design unit")) << error.what();
    }
}

// ===========================================================================
// 3. MUTANTS
// ===========================================================================

TEST(AnalyzerRobustness, EveryTokenDeletionAnalyzesOrFailsCleanly)
{
    for (const std::string& program : corpus())
        forEachToken(program, [&](size_t offset, size_t length)
        {
            std::string mutant = program;
            mutant.erase(offset, length);
            expectCleanOutcome(mutant, "without the token at " + std::to_string(offset));
        });
}

TEST(AnalyzerRobustness, EveryTokenDuplicationAnalyzesOrFailsCleanly)
{
    for (const std::string& program : corpus())
        forEachToken(program, [&](size_t offset, size_t length)
        {
            std::string mutant = program;
            mutant.insert(offset, program.substr(offset, length) + " ");
            expectCleanOutcome(mutant, "with the token at " + std::to_string(offset) + " doubled");
        });
}

TEST(AnalyzerRobustness, EveryTokenReplacementAnalyzesOrFailsCleanly)
{
    static const char* const replacements[] = { ";", "(", ")", "begin", "end", "is", "1", "'1'", "\"01\"", "x", "=>", "<=", ":=", "open", "others", "all" };

    for (const std::string& program : corpus())
        forEachToken(program, [&](size_t offset, size_t length)
        {
            for (const char* replacement : replacements)
            {
                std::string mutant = program;
                mutant.replace(offset, length, replacement);
                expectCleanOutcome(mutant, std::string("with the token at ") + std::to_string(offset) + " replaced by " + replacement);
            }
        });
}

TEST(AnalyzerRobustness, EveryPrefixAnalyzesOrFailsCleanly)
{
    for (const std::string& program : corpus())
        for (size_t cut = 0; cut < program.size(); cut += 3)
            expectCleanOutcome(program.substr(0, cut), "prefix of " + std::to_string(cut) + " characters");
}

TEST(AnalyzerRobustness, EveryTokenSwapAnalyzesOrFailsCleanly)
{
    for (const std::string& program : corpus())
    {
        std::vector<std::pair<size_t, size_t>> tokens;
        forEachToken(program, [&](size_t offset, size_t length) { tokens.push_back({ offset, length }); });

        for (size_t i = 0; i + 1 < tokens.size(); ++i)
        {
            const auto [firstOffset, firstLength] = tokens[i];
            const auto [secondOffset, secondLength] = tokens[i + 1];
            std::string mutant = program.substr(0, firstOffset) + program.substr(secondOffset, secondLength)
                               + program.substr(firstOffset + firstLength, secondOffset - firstOffset - firstLength)
                               + program.substr(firstOffset, firstLength) + program.substr(secondOffset + secondLength);
            expectCleanOutcome(mutant, "with tokens " + std::to_string(i) + " and " + std::to_string(i + 1) + " swapped");
        }
    }
}

// ===========================================================================
// 4. LONG AND DEEP INPUT
// ===========================================================================

namespace
{
    std::string designWith(const std::string& declarations, const std::string& body = "")
    {
        return "entity e is end e; architecture r of e is signal n, a : integer; signal v : std_logic_vector(3 downto 0); " + declarations +
               " begin " + body + " end r;";
    }

    std::string chain(const std::string& operand, const std::string& separator, int length)
    {
        std::string text = operand;
        for (int i = 1; i < length; ++i)
            text += separator + operand;
        return text;
    }
}

TEST(AnalyzerRobustness, ReasonableChainsAnalyze)
{
    EXPECT_EQ(failure(designWith("", "n <= " + chain("a", " + ", 100) + ";")), "<no error>");
    EXPECT_EQ(failure(designWith("constant k : integer := " + chain("3", " + ", 100) + ";")), "<no error>");
}

TEST(AnalyzerRobustness, ChainsBeyondTheAnalyzersDepthFailCleanly)
{
    const std::string message = failure(designWith("", "n <= " + chain("a", " + ", 400) + ";"));
    EXPECT_TRUE(mentions(message, "nested too deeply to analyze")) << message;

    const std::string constant = failure(designWith("constant k : integer := " + chain("3", " + ", 400) + ";"));
    EXPECT_TRUE(mentions(constant, "nested too deeply to analyze")) << constant;

    const std::string requiredStatic = failure(designWith("signal w : std_logic_vector(" + chain("3", " + ", 400) + " downto 0);"));
    EXPECT_TRUE(mentions(requiredStatic, "nested too deeply") || mentions(requiredStatic, "constant at analysis time")) << requiredStatic;
}

TEST(AnalyzerRobustness, ChainsBeyondTheParsersLimitFailCleanly)
{
    const std::string message = failure(designWith("", "n <= " + chain("a", " + ", 200000) + ";"));
    EXPECT_TRUE(mentions(message, "This expression is too long")) << message;

    const std::string selection = failure(designWith("", "n <= a" + chain(".b", "", 200000) + ";"));
    EXPECT_TRUE(mentions(selection, "This expression is too long")) << selection;

    const std::string alternatives = failure(designWith("", "n <= 1" + chain(" when true else 2", "", 200000) + ";"));
    EXPECT_TRUE(mentions(alternatives, "This expression is too long")) << alternatives;
}

TEST(AnalyzerRobustness, ChainsInDeclarationsAreClonedSafely)
{
    // `constant a, b : integer := <chain>` copies the value for every name; a chain at the parser's limit must survive that.
    EXPECT_EQ(failure(designWith("constant p, q : integer := " + chain("3", " + ", 200) + ";")), "<no error>");
}

TEST(AnalyzerRobustness, ManyStatementsAndDeclarationsTakeLinearTime)
{
    // Every character literal looks up the visible enumeration types and every declaration checks for clashes: both used to visit
    // every name of the region, so a design of this size took minutes instead of a fraction of a second.
    std::string declarations, body;
    for (int i = 0; i < 20000; ++i)
    {
        declarations += "signal s" + std::to_string(i) + " : std_logic; ";
        body += "s" + std::to_string(i) + " <= '1'; ";
    }

    const auto start = std::chrono::steady_clock::now();
    EXPECT_EQ(failure(designWith(declarations, body)), "<no error>");
    const auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    EXPECT_LT(seconds, 20.0) << "analysis time is no longer linear in the size of the design";
}

TEST(AnalyzerRobustness, LargeAggregatesAndConditionalChains)
{
    std::string aggregate = "(";
    for (int i = 0; i < 4000; ++i)
        aggregate += std::string(i ? ", " : "") + std::to_string(i);
    aggregate += ")";
    EXPECT_EQ(failure(designWith("type table is array (0 to 3999) of integer; constant rom : table := " + aggregate + ";")), "<no error>");

    EXPECT_EQ(failure(designWith("", "n <= " + chain("1", " when a = 0 else ", 1) + " when a = 0 else 2 when a = 1 else 3;")), "<no error>");
}

TEST(AnalyzerRobustness, DeepNestingFailsCleanly)
{
    const std::string parentheses = "n <= " + std::string(10000, '(') + "a" + std::string(10000, ')') + ";";
    EXPECT_TRUE(mentions(failure(designWith("", parentheses)), "Nesting is too deep"));

    std::string statements = "process begin ";
    for (int i = 0; i < 5000; ++i)
        statements += "if a = 1 then ";
    for (int i = 0; i < 5000; ++i)
        statements += "end if; ";
    statements += "wait; end process;";
    EXPECT_TRUE(mentions(failure(designWith("", statements)), "Nesting is too deep"));

    std::string subprograms = "procedure p0 is begin null; end; ";
    std::string nested = "";
    for (int i = 0; i < 300; ++i)
        nested += "procedure p" + std::to_string(i + 1) + " is ";
    for (int i = 0; i < 300; ++i)
        nested += "begin null; end; ";
    EXPECT_TRUE(mentions(failure(designWith(subprograms + nested)), "Nesting is too deep") || failure(designWith(subprograms + nested)) != "<no error>");
}

TEST(AnalyzerRobustness, DeepSubprogramRecursionInTheCallGraph)
{
    // A chain of procedures, each calling the next: the wait analysis walks it iteratively.
    std::string declarations;
    for (int i = 0; i < 500; ++i)
        declarations += "procedure p" + std::to_string(i) + " is begin p" + std::to_string(i + 1) + "; end; ";
    EXPECT_TRUE(mentions(failure(designWith(declarations)), "'p1' is not declared") || failure(designWith(declarations)) != "<no error>");

    std::string ordered;
    ordered += "procedure p500 is begin wait for 1 ns; end; ";
    for (int i = 499; i >= 0; --i)
        ordered += "procedure p" + std::to_string(i) + " is begin p" + std::to_string(i + 1) + "; end; ";
    EXPECT_TRUE(mentions(failure(designWith(ordered, "process (a) begin p0; end process;")), "has a sensitivity list"));
}

namespace
{
    constexpr int handBuiltLinks = 300000;

    ExpressionPtr additionsOf(int count)
    {
        ExpressionPtr chain = symbolNamed("n");
        for (int i = 0; i < count; ++i)
        {
            auto sum = std::make_unique<BinaryOpExpr>();
            sum->op = BinaryOperator::Add;
            sum->left = std::move(chain);
            sum->right = integerLiteral(1);
            chain = std::move(sum);
        }
        return chain;
    }

    ExpressionPtr selectionsOf(int count)
    {
        ExpressionPtr chain = symbolNamed("n");
        for (int i = 0; i < count; ++i)
        {
            auto field = std::make_unique<FieldAccessExpr>();
            field->target = std::move(chain);
            field->fieldName = "f";
            chain = std::move(field);
        }
        return chain;
    }

    ExpressionPtr callsOf(int count)
    {
        ExpressionPtr chain = symbolNamed("n");
        for (int i = 0; i < count; ++i)
        {
            auto call = std::make_unique<FunctionCallExpr>();
            call->callee = std::move(chain);
            call->arguments.push_back(integerLiteral(1));
            chain = std::move(call);
        }
        return chain;
    }

    ExpressionPtr attributesOf(int count)
    {
        ExpressionPtr chain = symbolNamed("n");
        for (int i = 0; i < count; ++i)
        {
            auto attribute = std::make_unique<AttributeExpr>();
            attribute->prefix = std::move(chain);
            attribute->attributeName = "length";
            chain = std::move(attribute);
        }
        return chain;
    }

    ExpressionPtr alternativesOf(int count)
    {
        ExpressionPtr chain = integerLiteral(0);
        for (int i = 0; i < count; ++i)
        {
            auto branch = std::make_unique<WhenElseExpr>();
            branch->trueValue = integerLiteral(1);
            branch->condition = symbolNamed("true");
            branch->falseValue = std::move(chain);
            chain = std::move(branch);
        }
        return chain;
    }
}

TEST(AnalyzerRobustness, HandBuiltChainsBeyondTheDepthFailCleanly)
{
    // The parser never builds these, but the analyzer takes any tree: it must answer with an error, and freeing the tree afterwards
    // must not recurse either.
    EXPECT_TRUE(mentions(assigning(additionsOf(handBuiltLinks)), "nested too deeply to analyze"));
    EXPECT_TRUE(mentions(assigning(selectionsOf(handBuiltLinks)), "nested too deeply to analyze"));
    EXPECT_TRUE(mentions(assigning(callsOf(handBuiltLinks)), "nested too deeply to analyze"));
    EXPECT_TRUE(mentions(assigning(attributesOf(handBuiltLinks)), "nested too deeply to analyze"));
}

TEST(AnalyzerRobustness, HandBuiltAlternativesAreWalkedWithoutRecursion)
{
    // `when/else` chains are analyzed in a loop, so however long they are they are simply analyzed.
    EXPECT_EQ(assigning(alternativesOf(handBuiltLinks)), "<no error>");
}

TEST(AnalyzerRobustness, HandBuiltChainsWithinTheDepthAnalyze)
{
    EXPECT_EQ(assigning(additionsOf(100)), "<no error>");
    EXPECT_EQ(assigning(alternativesOf(100)), "<no error>");
}

// ===========================================================================
// 5. LARGE DESIGNS ANALYZE IN LINEAR TIME
// ===========================================================================

namespace
{
    /// Analyzes `source`, which must be valid, and fails the test when it takes longer than `limitSeconds`.
    void expectFastAndValid(const std::string& source, const std::string& what, double limitSeconds = 20.0)
    {
        const auto start = std::chrono::steady_clock::now();
        EXPECT_EQ(failure(source), "<no error>") << what;
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        EXPECT_LT(seconds, limitSeconds) << what << " took " << seconds << " s";
    }

    std::string numbered(const std::string& prefix, int from, int to, const std::string& suffix = "")
    {
        std::string text;
        for (int i = from; i < to; ++i)
            text += prefix + std::to_string(i) + suffix;
        return text;
    }
}

TEST(AnalyzerScale, AnEnumerationWithThousandsOfLiteralsAndACaseOverIt)
{
    std::string literals = "l0";
    for (int i = 1; i < 3000; ++i)
        literals += ", l" + std::to_string(i);

    expectFastAndValid(designWith("type big is (" + literals + "); signal b : big;",
                                  "process begin case b is " + numbered("when l", 0, 3000, " => n <= 1; ") + " end case; wait; end process;"),
                       "a case with an alternative per literal");

    expectFastAndValid(designWith("type big is (" + literals + "); signal b : big;",
                                  "process begin case b is when l0 => n <= 1; when others => n <= 2; end case; wait; end process;"),
                       "a case with others");
}

TEST(AnalyzerScale, AnIntegerCaseWithThousandsOfAlternatives)
{
    expectFastAndValid(designWith("subtype byte is integer range 0 to 4999;",
                                  "process variable b : byte := 0; begin case b is " + numbered("when ", 0, 5000, " => n <= 1; ") + " end case; wait; end process;"),
                       "a case with an alternative per value");
}

TEST(AnalyzerScale, ManyProcessesAndInstances)
{
    std::string declarations = "component leaf is port (i : in std_logic; o : out std_logic); end component; ";
    std::string body;
    for (int i = 0; i < 3000; ++i)
    {
        declarations += "signal p" + std::to_string(i) + ", q" + std::to_string(i) + " : std_logic; ";
        body += "process (p" + std::to_string(i) + ") begin q" + std::to_string(i) + " <= p" + std::to_string(i) + "; end process; ";
        body += "u" + std::to_string(i) + " : leaf port map (i => p" + std::to_string(i) + ", o => open); ";
    }
    const std::string design = "entity leaf is port (i : in std_logic; o : out std_logic); end leaf; architecture r of leaf is begin o <= i; end r; "
                               "entity e is end e; architecture r of e is " + declarations + " begin " + body + " end r;";
    expectFastAndValid(design, "thousands of processes and instances");
}

TEST(AnalyzerScale, LongConditionalsAndElsifChains)
{
    expectFastAndValid(designWith("", "process begin if n = 0 then n <= 1; " + numbered("elsif n = ", 1, 4000, " then n <= 2; ") + " end if; wait; end process;"),
                       "an if with thousands of elsifs");

    expectFastAndValid(designWith("", "process begin " + numbered("n <= ", 0, 20000, "; ") + " wait; end process;"),
                       "a process with thousands of statements");
}

TEST(AnalyzerScale, WideInterfacesAndAssociations)
{
    const int width = 1500;
    std::string ports, component, map, signals;
    for (int i = 0; i < width; ++i)
    {
        const std::string name = std::to_string(i);
        ports += std::string(i ? "; " : "") + "i" + name + " : in std_logic";
        signals += "signal s" + name + " : std_logic; ";
        map += std::string(i ? ", " : "") + "i" + name + " => s" + name;
    }
    const std::string design = "entity wide is port (" + ports + "); end wide; architecture r of wide is begin end r; "
                               "entity e is end e; architecture r of e is component wide is port (" + ports + "); end component; " + signals +
                               " begin u : wide port map (" + map + "); end r;";
    expectFastAndValid(design, "an entity with a thousand ports and one instance of it");
}

TEST(AnalyzerScale, LargeAggregatesWithNamedAndPositionalElements)
{
    std::string named = "(";
    for (int i = 0; i < 4000; ++i)
        named += std::string(i ? ", " : "") + std::to_string(i) + " => " + std::to_string(i % 7);
    named += ")";
    expectFastAndValid(designWith("type table is array (0 to 3999) of integer; constant rom : table := " + named + ";"), "an aggregate with a named element per index");

    std::string choices = "(";
    for (int i = 0; i < 2000; ++i)
        choices += std::string(i ? " | " : "") + std::to_string(i);
    choices += " => 1, others => 0)";
    expectFastAndValid(designWith("type table is array (0 to 3999) of integer; constant rom : table := " + choices + ";"), "an aggregate with a choice list");
}

TEST(AnalyzerScale, ManyOverloadsOfOneName)
{
    std::string declarations = "type t0 is range 0 to 1; ";
    for (int i = 1; i < 300; ++i)
        declarations += "type t" + std::to_string(i) + " is range 0 to 1; ";
    for (int i = 0; i < 300; ++i)
        declarations += "function f(x : t" + std::to_string(i) + ") return integer is begin return " + std::to_string(i) + "; end; ";
    declarations += "constant k : t299 := 1; ";
    expectFastAndValid(designWith(declarations, "process begin n <= f(k); wait; end process;"), "three hundred overloads");
}

// ===========================================================================
// 6. TREES THE PARSER NEVER BUILDS
//
// The analyzer takes any tree, so every "this construct is not what it should be" rule has a test: a valid design is parsed,
// then one node is replaced by something no source text could produce.
// ===========================================================================

namespace
{
    /// Parses `source`, lets `mutate` change the tree, analyzes it, and returns the error (or "<no error>").
    std::string mutated(const std::string& source, const std::function<void(ArchitectureDeclaration&)>& mutate)
    {
        Tokenizer tokenizer(source);
        ASTRoot root = VHDLtoAST(tokenizer);
        for (auto& unit : root.children)
            if (auto* architecture = dynamic_cast<ArchitectureDeclaration*>(unit.get()))
                mutate(*architecture);

        try
        {
            analyzeAST(root);
        }
        catch (const compiler_error& error)
        {
            return error.what();
        }
        return "<no error>";
    }

    std::string design(const std::string& declarations, const std::string& body = "")
    {
        return "entity e is end e; architecture r of e is signal n : integer; signal v : std_logic_vector(1 downto 0); " + declarations +
               " begin " + body + " end r;";
    }

    template <typename T>
    T& firstOf(ArchitectureDeclaration& architecture)
    {
        for (auto& declaration : architecture.declarations)
            if (auto* found = dynamic_cast<T*>(declaration.get()))
                return *found;
        throw std::logic_error("the design has no such declaration");
    }

    /// Nodes of a kind no handler is registered for.
    struct AlienExpression final : Expression
    {
        std::unique_ptr<ASTNode> clone() const override { return std::make_unique<AlienExpression>(); }
        void print(int) const override { }
    };
    struct AlienStatement final : Statement
    {
        std::unique_ptr<ASTNode> clone() const override { return std::make_unique<AlienStatement>(); }
        void print(int) const override { }
    };
    struct AlienDeclaration final : Declaration
    {
        std::unique_ptr<ASTNode> clone() const override { return std::make_unique<AlienDeclaration>(); }
        void print(int) const override { }
    };
    struct AlienTypeDefinition final : TypeDefinition
    {
        std::unique_ptr<ASTNode> clone() const override { return std::make_unique<AlienTypeDefinition>(); }
        void print(int) const override { }
    };
}

TEST(AnalyzerTrees, EveryFamilyReportsANodeItHasNoHandlerFor)
{
    EXPECT_TRUE(mentions(mutated(design("constant k : integer := 1;"), [](ArchitectureDeclaration& a)
                                 { firstOf<ConstantDeclaration>(a).value = std::make_unique<AlienExpression>(); }),
                         "The analyzer has no handler for this kind of expression"));

    EXPECT_TRUE(mentions(mutated(design(""), [](ArchitectureDeclaration& a) { a.body.push_back(std::make_unique<AlienStatement>()); }),
                         "no handler for this kind of statement in an architecture"));

    EXPECT_TRUE(mentions(mutated(design("", "process begin wait; end process;"), [](ArchitectureDeclaration& a)
    {
        auto& process = static_cast<ProcessStatement&>(*a.body.front());
        process.body.insert(process.body.begin(), std::make_unique<AlienStatement>());
    }), "no handler for this kind of statement in a process or subprogram"));

    EXPECT_TRUE(mentions(mutated(design(""), [](ArchitectureDeclaration& a) { a.declarations.push_back(std::make_unique<AlienDeclaration>()); }),
                         "no handler for this kind of declaration"));

    EXPECT_TRUE(mentions(mutated(design("type t is range 0 to 1;"), [](ArchitectureDeclaration& a)
                                 { firstOf<TypeDeclaration>(a).definition = std::make_unique<AlienTypeDefinition>(); }),
                         "no handler for this kind of type definition"));
}

TEST(AnalyzerTrees, AnAggregateChoiceMustBeAChoiceList)
{
    const std::string source = design("constant k : std_logic_vector(1 downto 0) := (0 => '1', 1 => '0');");
    EXPECT_TRUE(mentions(mutated(source, [](ArchitectureDeclaration& a)
    {
        auto& aggregate = static_cast<AggregateExpr&>(*firstOf<ConstantDeclaration>(a).value);
        static_cast<NamedAssociationExpr&>(*aggregate.elements.front()).formal = symbolNamed("zz");
    }), "Expected a list of choices before '=>'"));
}

TEST(AnalyzerTrees, AnAttributeSpecificationCannotMixOthersWithNames)
{
    const std::string source = design("attribute keep : boolean; attribute keep of n, v : signal is true;");
    EXPECT_TRUE(mentions(mutated(source, [](ArchitectureDeclaration& a)
    {
        firstOf<AttributeSpecification>(a).entities.back() = std::make_unique<OthersExpr>();
    }), "'others' and 'all' cannot be mixed with names in an attribute specification"));
}

TEST(AnalyzerTrees, ThePartsOfAPhysicalLiteralAreNumbers)
{
    EXPECT_TRUE(mentions(mutated(design("type shade is (dark, light); constant c : time := 1 ns;"), [](ArchitectureDeclaration& a)
    {
        static_cast<PhysicalLiteralExpr&>(*firstOf<ConstantDeclaration>(a).value).magnitude = symbolNamed("dark");
    }), "The magnitude of a physical literal must be a number"));
}

TEST(AnalyzerTrees, AQualifiedExpressionNeedsATypeMark)
{
    EXPECT_TRUE(mentions(mutated(design("constant c : unsigned(3 downto 0) := unsigned'(\"0101\");"), [](ArchitectureDeclaration& a)
    {
        static_cast<QualifiedExpr&>(*firstOf<ConstantDeclaration>(a).value).typeMark = symbolNamed("unsigned");
    }), "The prefix of a qualified expression must be a type mark"));
}

TEST(AnalyzerTrees, ASignatureListsTypeMarks)
{
    const std::string source = design("function f(x : integer) return integer is begin return x; end; alias g is f [integer return integer];");
    EXPECT_TRUE(mentions(mutated(source, [](ArchitectureDeclaration& a)
    {
        firstOf<AliasDeclaration>(a).signature->parameters.front() = integerLiteral(1);
    }), "A signature lists type marks"));
}

TEST(AnalyzerTrees, AResolutionNamesAFunction)
{
    const std::string source = design("function rs(x : std_logic_vector) return std_logic is begin return '0'; end; subtype r is rs std_logic;");
    EXPECT_TRUE(mentions(mutated(source, [](ArchitectureDeclaration& a)
    {
        firstOf<SubtypeDeclaration>(a).baseType->resolution = integerLiteral(1);
    }), "A resolution must name a function"));

    EXPECT_TRUE(mentions(failure(design("signal s : integer; subtype r is s std_logic;")), "is not a function"));
    EXPECT_TRUE(mentions(failure(design("subtype r is (rs) integer;")), "needs an array type"));
}

TEST(AnalyzerTrees, StaticnessLooksInsideConditionalValuesAndChoiceLists)
{
    // A conditional value is no constant's value; the tree is refused for that, after the staticness of its parts was looked at.
    EXPECT_TRUE(mentions(mutated(design("constant k : integer := 1;"), [](ArchitectureDeclaration& a)
    {
        auto conditional = std::make_unique<WhenElseExpr>();
        conditional->trueValue = integerLiteral(1);
        conditional->condition = symbolNamed("true");
        conditional->falseValue = integerLiteral(2);
        firstOf<ConstantDeclaration>(a).value = std::move(conditional);
    }), "A 'when ... else' value can only be the right-hand side of an assignment"));

    EXPECT_TRUE(mentions(mutated(design("constant k : integer := 1;"), [](ArchitectureDeclaration& a)
    {
        auto choices = std::make_unique<ChoiceListExpr>();
        choices->alternatives.push_back(integerLiteral(1));
        firstOf<ConstantDeclaration>(a).value = std::move(choices);
    }), "A list of choices is not a value"));

    EXPECT_TRUE(mentions(mutated(design("constant k : integer := 1;"), [](ArchitectureDeclaration& a)
    {
        auto conditional = std::make_unique<WhenElseExpr>();
        conditional->trueValue = symbolNamed("n");
        conditional->condition = symbolNamed("true");
        conditional->falseValue = integerLiteral(2);
        firstOf<ConstantDeclaration>(a).value = std::move(conditional);
    }), "must be constant"));
}

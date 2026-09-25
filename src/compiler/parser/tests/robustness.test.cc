// robustness.test.cc — the parser never crashes, hangs or throws anything but a compiler_error.
//
// Every valid program of the corpus is mutated (cut at every character, every token deleted, every token
// doubled). A mutant may parse or fail, but a failure must be a compiler_error with a location inside the
// source: never a crash, a null dereference or a foreign exception (std::out_of_range, std::bad_alloc ...).

#include "parser_test_util.h"

#include <functional>

using namespace ParserTest;

namespace
{
    const std::vector<std::string>& corpus()
    {
        static const std::vector<std::string> programs = {
            // test-project/clock.vhd
            "ENTITY clock IS\n"
            "    PORT (\n"
            "        clk_out : OUT STD_LOGIC\n"
            "    );\n"
            "END ENTITY clock;\n"
            "ARCHITECTURE behavioral OF clock IS\n"
            "    SIGNAL clk_out_internal : STD_LOGIC := '0';\n"
            "BEGIN\n"
            "    clk_out <= clk_out_internal;\n"
            "    PROCESS\n"
            "    BEGIN\n"
            "        IF clk_out_internal = '0' THEN\n"
            "            clk_out_internal <= '1';\n"
            "        ELSE\n"
            "            clk_out_internal <= '0';\n"
            "        END IF;\n"
            "        WAIT FOR 7 fs;\n"
            "    END PROCESS;\n"
            "END ARCHITECTURE behavioral;",

            // test-project/counter.vhd
            "ENTITY counter IS\n"
            "    PORT (\n"
            "        clk : IN STD_LOGIC;\n"
            "        reset : IN STD_LOGIC;\n"
            "        count : OUT UNSIGNED(31 DOWNTO 0)\n"
            "    );\n"
            "END ENTITY counter;\n"
            "ARCHITECTURE behavioral OF counter IS\n"
            "    SIGNAL count_internal : UNSIGNED(31 DOWNTO 0);\n"
            "BEGIN\n"
            "    PROCESS (clk, reset)\n"
            "    BEGIN\n"
            "        IF reset = '1' THEN\n"
            "            count_internal <= x\"00000000\";\n"
            "        ELSIF clk'event AND clk = '1' THEN\n"
            "            count_internal <= count_internal + 1;\n"
            "        END IF;\n"
            "    END PROCESS;\n"
            "    count <= count_internal;\n"
            "END ARCHITECTURE behavioral;",

            // test-project/counter-circuit.vhd
            "ENTITY top IS\n"
            "END ENTITY top;\n"
            "ARCHITECTURE behavioral OF top IS\n"
            "    COMPONENT clock\n"
            "        PORT (clk_out : OUT STD_LOGIC);\n"
            "    END COMPONENT;\n"
            "    COMPONENT counter\n"
            "        PORT (clk : IN STD_LOGIC; reset : IN STD_LOGIC; count : OUT UNSIGNED(31 DOWNTO 0));\n"
            "    END COMPONENT;\n"
            "    SIGNAL reset : STD_LOGIC := '1';\n"
            "    SIGNAL clk: STD_LOGIC;\n"
            "    SIGNAL count : UNSIGNED(31 DOWNTO 0);\n"
            "BEGIN\n"
            "    clk_inst : clock PORT MAP(clk_out => clk);\n"
            "    counter_inst : counter PORT MAP(clk => clk, reset => reset, count => count);\n"
            "    PROCESS\n"
            "    BEGIN\n"
            "        WAIT FOR 1 fs;\n"
            "        reset <= '0';\n"
            "        WAIT;\n"
            "    END PROCESS;\n"
            "END ARCHITECTURE behavioral;",

            // Declarations and types
            "library ieee; use ieee.std_logic_1164.all;\n"
            "entity e is generic (n : natural := 4); port (a, b : in std_logic; y : out std_logic_vector(7 downto 0) := (others => '0')); end entity e;\n"
            "architecture rtl of e is\n"
            "    subtype nib is (resolved) std_ulogic_vector(3 downto 0);\n"
            "    type color is (red, green, 'x');\n"
            "    type mem is array (0 to 3) of nib;\n"
            "    type vec is array (natural range <>) of bit;\n"
            "    type rec is record x, y : integer; z : nib; end record rec;\n"
            "    type dist is range 0 to 1e6 units um; mm = 1000 um; end units;\n"
            "    constant c : integer := 16#FF# + 1_000;\n"
            "    signal s : mem;\n"
            "    alias hi : bit_vector(3 downto 0) is w(7 downto 4);\n"
            "    attribute keep : boolean;\n"
            "    attribute keep of s : signal is true;\n"
            "    component adder is generic (w : natural); port (a : in bit; s : out bit); end component;\n"
            "    function \"and\"(l, r : bit) return bit;\n"
            "    impure function f(constant k : integer := 1) return integer is variable t : integer; begin return t + k; end function f;\n"
            "    procedure p(signal q : out bit) is begin q <= '1'; return; end procedure;\n"
            "begin\n"
            "    y <= x\"A5\" when a = '1' else 8sx\"F\" when b = '0' else (others => '0');\n"
            "    with c select? y <= \"00000001\" when 0 | 1, unaffected when others;\n"
            "    u1 : adder generic map (w => 8) port map (a => a, s => open);\n"
            "    u2 : component adder port map (a, b);\n"
            "    chk : assert n > 0 report \"n\" severity failure;\n"
            "    p(y(0));\n"
            "end architecture rtl;",

            // Sequential statements and expressions
            "entity e is end e;\n"
            "architecture a of e is\n"
            "    signal x : integer range 0 to 15;\n"
            "begin\n"
            "    main : process (all)\n"
            "        variable v : integer := 0;\n"
            "    begin\n"
            "        if x = 1 then v := v + 1; elsif x = 2 then v := -v; else null; end if;\n"
            "        case x is when 0 | 1 => v := 2; when 2 to 3 => v := 3; when others => null; end case;\n"
            "        case? x is when 1 => null; when others => null; end case?;\n"
            "        for i in 7 downto 0 loop exit when i = 3; end loop;\n"
            "        l : while v < 10 loop next l when v = 2; v := v * 2 ** 2; end loop l;\n"
            "        loop exit; end loop;\n"
            "        assert v > 0 report \"positive\" severity warning;\n"
            "        report \"done\";\n"
            "        v := abs v mod 3 + (and x) + t'(1) + f(a => 1)(2) + r.all.x + v'length;\n"
            "        v := <<variable .e.main.v : integer>> when ?? en else v;\n"
            "        x <= 1 when v = 0 else 2;\n"
            "        wait on x until x = 1 for 1.5 us;\n"
            "    end process main;\n"
            "end a;",
        };
        return programs;
    }

    /// Parses `source` and reports anything that is not a success or a well-located compiler_error.
    void expectCleanOutcome(const std::string& source, const std::string& what)
    {
        try
        {
            ASTRoot root = parseSource(source);
            (void)dump(root);
        }
        catch (const compiler_error& e)
        {
            EXPECT_GE(e.location().line, 1u) << what;
            EXPECT_GE(e.location().column, 1u) << what;
        }
        catch (const std::exception& e)
        {
            ADD_FAILURE() << what << ": foreign exception: " << e.what() << "\n" << source;
        }
        catch (...)
        {
            ADD_FAILURE() << what << ": unknown exception\n" << source;
        }
    }

    /// Byte offset of a token inside `source`, from its 1-based line and column.
    size_t offsetOf(const std::string& source, const Token& token)
    {
        size_t offset = 0;
        for (size_t line = 1; line < token.line; ++line)
            offset = source.find('\n', offset) + 1;
        return offset + token.column - 1;
    }

    /// Calls `mutate(offset, length)` for every token of `source`.
    void forEachToken(const std::string& source, const std::function<void(size_t, size_t)>& mutate)
    {
        Tokenizer tokenizer(source);
        for (size_t i = 0; i < tokenizer.size(); ++i)
        {
            const Token& token = *tokenizer.peek(static_cast<ssize_t>(i));
            mutate(offsetOf(source, token), token.original.size());
        }
    }
}

// ===========================================================================
// 1. THE CORPUS IS VALID
// ===========================================================================

TEST(ParserRobustness, CorpusParses)
{
    for (const std::string& program : corpus())
        EXPECT_NO_THROW(parseSource(program)) << program;
}

TEST(ParserRobustness, TokenOffsetsAreExact)
{
    for (const std::string& program : corpus())
        forEachToken(program, [&](size_t offset, size_t length)
        {
            ASSERT_LE(offset + length, program.size());
        });
}

// ===========================================================================
// 2. MUTANTS
// ===========================================================================

TEST(ParserRobustness, EveryPrefixParsesOrFailsCleanly)
{
    for (const std::string& program : corpus())
        for (size_t cut = 0; cut < program.size(); ++cut)
            expectCleanOutcome(program.substr(0, cut), "prefix of " + std::to_string(cut) + " characters");
}

TEST(ParserRobustness, EveryTokenDeletionParsesOrFailsCleanly)
{
    for (const std::string& program : corpus())
        forEachToken(program, [&](size_t offset, size_t length)
        {
            std::string mutant = program;
            mutant.erase(offset, length);
            expectCleanOutcome(mutant, "without the token at " + std::to_string(offset));
        });
}

TEST(ParserRobustness, EveryTokenDuplicationParsesOrFailsCleanly)
{
    for (const std::string& program : corpus())
        forEachToken(program, [&](size_t offset, size_t length)
        {
            std::string mutant = program;
            mutant.insert(offset, program.substr(offset, length) + " ");
            expectCleanOutcome(mutant, "with the token at " + std::to_string(offset) + " doubled");
        });
}

TEST(ParserRobustness, TruncatedProgramsAreNeverAccepted)
{
    // clock.vhd is an entity followed by an architecture. A cut is only a valid file right after the entity;
    // anywhere else it leaves an unfinished unit, which must be an error.
    const std::string& program = corpus().front();
    const size_t entityEnd = program.find("END ENTITY clock;") + std::string("END ENTITY clock;").size();

    for (size_t cut = 1; cut < program.size(); ++cut)
    {
        const std::string prefix = program.substr(0, cut);
        const bool justTheEntity = cut >= entityEnd && prefix.find_first_not_of(" \n", entityEnd) == std::string::npos;
        if (!justTheEntity)
        {
            EXPECT_TRUE(syntaxError(prefix).thrown) << "accepted a cut at " << cut << ":\n" << prefix;
        }
    }
}

// ===========================================================================
// 3. THE TREE
// ===========================================================================

TEST(ParserRobustness, ParsedTreesCloneExactly)
{
    for (const std::string& program : corpus())
    {
        ASTRoot root = parseSource(program);
        std::unique_ptr<ASTNode> copy = root.clone();
        EXPECT_EQ(dump(*copy), dump(root));
    }
}

TEST(ParserRobustness, ParsingIsDeterministic)
{
    for (const std::string& program : corpus())
        EXPECT_EQ(dump(parseSource(program)), dump(parseSource(program)));
}

TEST(ParserRobustness, GarbageFailsCleanly)
{
    for (const char* source : { ";", ")", "end", "begin end", "entity", "entity e", "entity e is", "architecture",
                                "entity e is port (a : in bit",  "architecture a of e is begin y <=",
                                "architecture a of e is begin process begin", "(((((", "<<<<", "'''", "=> => =>" })
        expectCleanOutcome(source, source);
}

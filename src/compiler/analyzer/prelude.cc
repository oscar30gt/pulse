#include "analyzer_internal.h"

#include "parser.h"
#include "tokenizer.h"

#include <string>
#include <unordered_map>
#include <utility>

namespace Pulse::Parser
{
    namespace
    {
        /// The predefined environment, written in VHDL and parsed with the real parser: the STANDARD
        /// types plus std_logic and the numeric_std vectors. Supporting another predefined type
        /// (bit, character, string, ...) only means adding its declaration here.
        const char* const kPreludeSource = R"(
            type boolean is (false, true);
            type severity_level is (note, warning, error, failure);
            type std_logic is ('U', 'X', '0', '1', 'Z', 'W', 'L', 'H', '-');

            type integer is range -2147483648 to 2147483647;
            subtype natural is integer range 0 to 2147483647;
            subtype positive is integer range 1 to 2147483647;
            type real is range -1.0e308 to 1.0e308;

            type time is range -9223372036854775807 to 9223372036854775807
                units
                    fs;
                    ps  = 1000 fs;
                    ns  = 1000 ps;
                    us  = 1000 ns;
                    ms  = 1000 us;
                    sec = 1000 ms;
                    min = 60 sec;
                    hr  = 60 min;
                end units;

            type std_logic_vector is array (natural range <>) of std_logic;
            type unsigned is array (natural range <>) of std_logic;
            type signed is array (natural range <>) of std_logic;
        )";

        /// The subprograms of the IEEE packages std_logic_1164 and numeric_std, as declarations without a body: the analyzer
        /// resolves calls and operators against them, and the elaborator implements them natively. The operators the LRM
        /// declares implicitly for every type (`=` and `<` on any type, integer arithmetic, `&` ...) are not here: the
        /// predefined operator rules handle them.
        std::string ieeeSignatures()
        {
            const char* const logical[] = { "and", "or", "nand", "nor", "xor", "xnor" };
            std::string text;

            const auto function = [&text](const std::string& designator, const std::string& parameters, const std::string& result)
            {
                text += "function " + designator + " (" + parameters + ") return " + result + ";\n";
            };
            const auto op = [](const std::string& symbol) { return "\"" + symbol + "\""; };

            // ---- std_logic_1164 (and numeric_std's logical operators on its vectors) ----
            for (const char* name : logical)
                function(op(name), "l, r : std_logic", "std_logic");
            function(op("not"), "l : std_logic", "std_logic");
            function(op("??"), "l : std_logic", "boolean");

            for (const char* vector : { "std_logic_vector", "unsigned", "signed" })
            {
                const std::string type = vector;
                for (const char* name : logical)
                {
                    function(op(name), "l, r : " + type, type);
                    function(op(name), "l : " + type, "std_logic");     // VHDL-2008 logical reduction
                }
                function(op("not"), "l : " + type, type);
            }

            for (const char* name : { "sll", "srl", "rol", "ror" })
                function(op(name), "l : std_logic_vector; r : integer", "std_logic_vector");

            function("rising_edge", "signal s : std_logic", "boolean");
            function("falling_edge", "signal s : std_logic", "boolean");

            // ---- numeric_std ----
            for (const auto& [vector, integer] : { std::pair<std::string, std::string>{ "unsigned", "natural" }, { "signed", "integer" } })
            {
                for (const char* name : { "+", "-", "*", "/", "mod", "rem" })
                {
                    function(op(name), "l, r : " + vector, vector);
                    function(op(name), "l : " + vector + "; r : " + integer, vector);
                    function(op(name), "l : " + integer + "; r : " + vector, vector);
                }
                for (const char* name : { "=", "/=", "<", "<=", ">", ">=" })
                {
                    function(op(name), "l, r : " + vector, "boolean");
                    function(op(name), "l : " + vector + "; r : " + integer, "boolean");
                    function(op(name), "l : " + integer + "; r : " + vector, "boolean");
                }
                for (const char* name : { "sll", "srl", "sla", "sra", "rol", "ror" })
                    function(op(name), "arg : " + vector + "; count : integer", vector);
                for (const char* name : { "shift_left", "shift_right", "rotate_left", "rotate_right" })
                    function(name, "arg : " + vector + "; count : natural", vector);
                function("resize", "arg : " + vector + "; new_size : natural", vector);
            }

            function(op("-"), "arg : signed", "signed");
            function(op("abs"), "arg : signed", "signed");
            function("to_integer", "arg : unsigned", "natural");
            function("to_integer", "arg : signed", "integer");
            function("to_unsigned", "arg, size : natural", "unsigned");
            function("to_signed", "arg : integer; size : natural", "signed");
            return text;
        }

        /// Properties of predefined types that VHDL text cannot express.
        struct PreludeTrait
        {
            bool isResolved = false;
            VectorFamily family = VectorFamily::None;
        };

        const std::unordered_map<std::string, PreludeTrait>& preludeTraits()
        {
            static const std::unordered_map<std::string, PreludeTrait> traits = {
                { "std_logic",        { true,  VectorFamily::None } },
                { "std_logic_vector", { false, VectorFamily::StdLogicVector } },
                { "unsigned",         { false, VectorFamily::Unsigned } },
                { "signed",           { false, VectorFamily::Signed } },
            };
            return traits;
        }

        const TypeInfo* baseTypeNamed(const Scope& scope, const std::string& name)
        {
            auto found = scope.symbols.find(name);
            return found == scope.symbols.end() ? nullptr : found->second.type.info;
        }
    } // anonymous namespace

    void AnalyzerContext::applyPreludeTraits(TypeInfo& info) const
    {
        auto found = preludeTraits().find(info.name);
        if (found == preludeTraits().end())
            return;

        info.isResolved = info.isResolved || found->second.isResolved;
        info.family = found->second.family;
    }

    void AnalyzerContext::bindPreludeTypes()
    {
        const Scope& base = m_scopes.front();
        m_std.boolean = baseTypeNamed(base, "boolean");
        m_std.stdLogic = baseTypeNamed(base, "std_logic");
        m_std.time = baseTypeNamed(base, "time");
        m_std.integer = baseTypeNamed(base, "integer");
        m_std.severity = baseTypeNamed(base, "severity_level");
        m_rules = OperatorRules(m_std.boolean, m_std.stdLogic);
    }

    AnalyzerContext::AnalyzerContext(PreludeTag)
    {
        registerDispatchTables();
        registerExpressionHandlers();
        registerSequentialHandlers();

        static const std::vector<DeclarationPtr> declarations = []
        {
            Tokenizer tokenizer(std::string(kPreludeSource) + ieeeSignatures());
            return parseDeclarations(tokenizer);
        }();

        m_loadingPrelude = true;
        pushScope();
        analyzeDeclarations(declarations);
        m_loadingPrelude = false;

        bindPreludeTypes();
    }

    /// The prelude is analyzed once, into a context that lives until the program ends. Its types and builtin subprograms
    /// never change afterwards, so every library starts from a copy of its scope that refers to them.
    void AnalyzerContext::loadPrelude()
    {
        static const AnalyzerContext prelude{ PreludeTag{} };

        m_scopes = { prelude.m_scopes.front() };
        bindPreludeTypes();
    }

} // namespace Pulse::Parser

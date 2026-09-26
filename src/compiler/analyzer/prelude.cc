#include "analyzer_internal.h"

#include "parser.h"
#include "tokenizer.h"

#include <unordered_map>

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

    void AnalyzerContext::loadPrelude()
    {
        Tokenizer tokenizer(kPreludeSource);
        static const std::vector<DeclarationPtr> declarations = parseDeclarations(tokenizer);

        m_loadingPrelude = true;
        pushScope();
        analyzeDeclarations(declarations);
        m_loadingPrelude = false;

        bindPreludeTypes();
    }

} // namespace Pulse::Parser

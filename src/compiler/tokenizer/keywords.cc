#include "keywords.h"

#include <string_view>
#include <unordered_set>

namespace Pulse::Parser
{
    namespace
    {
        /// Reserved words that act as operators. `to`/`downto` are included because
        /// Pulse parser treats ranges as binary operators that return a range value.
        const std::unordered_set<std::string_view> operatorWords = {
            "abs", "and", "mod", "nand", "nor", "not", "or", "rem",
            "rol", "ror", "sla", "sll", "sra", "srl", "xnor", "xor",
            "downto", "to",
        };

        /// Every other VHDL-2008 reserved word.
        const std::unordered_set<std::string_view> keywordWords = {
            "access", "after", "alias", "all", "architecture", "array", "assert", "assume", "assume_guarantee",
            "attribute", "begin", "block", "body", "buffer", "bus", "case", "component", "configuration",
            "constant", "context", "cover", "default", "disconnect",
            "else", "elsif", "end", "entity", "exit", "fairness", "file", "for", "force", "function",
            "generate", "generic", "group", "guarded", "if", "impure", "in", "inertial", "inout", "is",
            "label", "library", "linkage", "literal", "loop", "map", "new", "next", "null", "of", "on",
            "open", "others", "out", "package", "parameter", "port", "postponed", "procedure", "process",
            "property", "protected", "pure",
            "range", "record", "register", "reject", "release", "report", "restrict", "restrict_guarantee",
            "return", "select", "sequence", "severity",
            "signal", "shared", "strong", "subtype", "then", "transport", "type", "unaffected", "units", "until",
            "use", "variable", "vmode", "vprop", "vunit", "wait", "when", "while", "with",
        };

    } // anonymous namespace

    TokenType classifyWord(const std::string& lowered)
    {
        if (operatorWords.count(lowered)) return TokenType::Operator;
        if (keywordWords.count(lowered))  return TokenType::Keyword;
        return TokenType::Identifier;
    }

} // namespace Pulse::Parser

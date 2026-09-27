#ifndef PULSE_PARSER_LIB_KEYWORDS_H
#define PULSE_PARSER_LIB_KEYWORDS_H

#include <string>
#include "tokenizer.h"

namespace Pulse::Parser
{
    /// Classifies a lowercased word: reserved words map to TokenType::Keyword, reserved words that
    /// behave as operators (and, mod, downto, ...) to TokenType::Operator, anything else to Identifier.
    /// The table covers the full VHDL-2008 reserved-word list so an unsupported construct is always
    /// recognised as a keyword instead of being mistaken for a user identifier.
    TokenType classifyWord(const std::string& lowered);

} // namespace Pulse::Parser

#endif // PULSE_PARSER_LIB_KEYWORDS_H

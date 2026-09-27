#include "source_reader.h"

#include <string>

namespace Pulse::Parser
{
    // ---- Multi-character operators ------------------------------------------

    bool SourceReader::tokenizeCompoundOperator()
    {
        static const char* const threeChar[] = { "?/=", "?<=", "?>=" };
        static const char* const twoChar[]   = { ":=", "<=", ">=", "=>", "/=", "**", "<>", "<<", ">>", "??", "?=", "?<", "?>" };

        SourceMark start = mark();

        for (const char* op : threeChar)
        {
            if (source.compare(index, 3, op) == 0)
            {
                advance(3);
                emit(TokenType::Operator, op, start);
                return true;
            }
        }

        for (const char* op : twoChar)
        {
            if (source.compare(index, 2, op) == 0)
            {
                advance(2);
                emit(TokenType::Operator, op, start);
                return true;
            }
        }
        return false;
    }

    // ---- Delimiters ---------------------------------------------------------

    bool SourceReader::tokenizeDelimiter()
    {
        static const std::string delimiters = "()[],;:.@^`";
        if (atEnd() || delimiters.find(current()) == std::string::npos)
            return false;

        SourceMark start = mark();
        advance();
        emit(TokenType::Delimiter, std::string(1, source[start.index]), start);
        return true;
    }

    // ---- Single-character operators -----------------------------------------

    bool SourceReader::tokenizeSingleCharOperator()
    {
        // A lone '?' is the VHDL-2008 delimiter of `case?` and `select?`.
        static const std::string operators = "&+-*/<>=|!?";
        if (atEnd() || operators.find(current()) == std::string::npos)
            return false;

        SourceMark start = mark();
        char c = current();
        advance();
        // '!' is the LRM replacement character for the vertical bar; normalize it so the parser only sees '|'.
        emit(TokenType::Operator, std::string(1, c == '!' ? '|' : c), start);
        return true;
    }

} // namespace Pulse::Parser

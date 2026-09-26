#include "analyzer_internal.h"

#include <algorithm>

namespace Pulse::Parser
{
    namespace
    {
        std::string joinTypeNames(const std::vector<const TypeInfo*>& types)
        {
            std::string text;
            for (const TypeInfo* type : types)
                text += (text.empty() ? "'" : ", '") + type->name + "'";
            return text;
        }
    } // anonymous namespace

    SemanticType AnalyzerContext::typeOfInteger(const IntegerLiteralExpr&, const SemanticType*)
    {
        return universalInteger();
    }

    SemanticType AnalyzerContext::typeOfReal(const DoubleLiteralExpr&, const SemanticType*)
    {
        return universalReal();
    }

    SemanticType AnalyzerContext::typeOfPhysical(const PhysicalLiteralExpr& expr, const SemanticType*)
    {
        const auto unit = findUnit(expr.unit);
        if (!unit)
            fail("Unknown unit '" + expr.unit + "'", expr);

        const SemanticType magnitude = exprType(*expr.magnitude);
        if (!isUniversal(magnitude))
            fail("The magnitude of a physical literal must be a number", expr);

        if (!fold(expr))
            fail("'" + expr.unit + "' quantity is not a whole number of the base unit '" + unit->type->units.front().name
                 + "' of type '" + unit->type->name + "', or it is too large", expr);

        return typeOf(*unit->type);
    }

    // ---- Enumeration and character literals -----------------------------------------------------

    SemanticType AnalyzerContext::typeOfCharacter(const CharacterLiteralExpr& expr, const SemanticType* expected)
    {
        return enumerationLiteralType(expr.value, expected, expr);
    }

    /// Type of an identifier that is not an object: an enumeration literal (chosen by context when
    /// several types declare it) or a physical unit. Also used for character literals.
    SemanticType AnalyzerContext::enumerationLiteralType(const std::string& literal, const SemanticType* expected, const ASTNode& node)
    {
        const auto owners = enumerationOwners(literal);
        const bool isCharacter = literal.front() == '\'';

        if (owners.empty())
        {
            if (!isCharacter)
                if (auto unit = findUnit(literal))
                    return typeOf(*unit->type);

            fail(isCharacter ? "Character literal " + literal + " is not a value of any visible type"
                             : "'" + literal + "' is not declared", node);
        }

        if (const TypeInfo* owner = pickEnumerationOwner(literal, expected))
            return typeOf(*owner);

        fail("'" + literal + "' is ambiguous: it is a value of " + joinTypeNames(owners)
             + (expected ? ", none of which is the expected type '" + describe(*expected) + "'"
                         : " and the context does not say which one is meant"), node);
    }

    // ---- String literals ------------------------------------------------------------------------

    namespace
    {
        /// The characters of a string (UTF-8 sequences stay together), each written as the character literal it stands for.
        std::vector<std::string> characterLiterals(const std::string& text)
        {
            std::vector<std::string> literals;
            for (size_t i = 0; i < text.size();)
            {
                const unsigned char lead = static_cast<unsigned char>(text[i]);
                const size_t length = std::min<size_t>(lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1, text.size() - i);
                literals.push_back("'" + text.substr(i, length) + "'");
                i += length;
            }
            return literals;
        }
    } // anonymous namespace

    SemanticType AnalyzerContext::typeOfString(const StringLiteralExpr& expr, const SemanticType* expected)
    {
        if (!expected || !isArray(*expected))
            fail(expected ? "A string literal cannot be used where '" + describe(*expected) + "' is expected"
                          : "The type of this string literal cannot be determined from its context; use it where a "
                            "string, std_logic_vector, unsigned or signed is expected", expr);

        if (!isOneDimensionalArray(*expected))
            fail("A string literal cannot initialize the multi-dimensional type '" + describe(*expected) + "'", expr);

        const TypeInfo& elementInfo = *expected->info->element.info;
        const std::vector<std::string> characters = characterLiterals(expr.value);

        for (const std::string& literal : characters)
        {
            if (std::find(elementInfo.literals.begin(), elementInfo.literals.end(), literal) == elementInfo.literals.end())
                fail("The literal contains " + literal + ", which is not a value of '" + elementInfo.name
                     + "', the element type of '" + expected->info->name + "'", expr);
        }

        const int64_t length = static_cast<int64_t>(characters.size());

        SemanticType result;
        result.info = expected->info;

        if (expected->dims.empty())
        {
            result.dims = { Bounds{ length - 1, 0, false } };
            return result;
        }

        if (expected->dims.front().length() != length)
            fail("The literal has " + std::to_string(length) + " element(s), but '" + describe(*expected)
                 + "' has " + std::to_string(expected->dims.front().length()), expr);

        result.dims = expected->dims;
        return result;
    }

} // namespace Pulse::Parser

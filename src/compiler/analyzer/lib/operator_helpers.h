#ifndef PULSE_COMPILER_OPERATOR_HELPERS_H
#define PULSE_COMPILER_OPERATOR_HELPERS_H

// Small predicates and message builders shared by the operator rule files and the analyzer.

#include "operator_rules.h"

namespace Pulse::Parser
{
    // ---- Operator classes -----------------------------------------------------------------------

    inline bool isLogicalOperator(BinaryOperator op)
    {
        switch (op)
        {
            case BinaryOperator::And: case BinaryOperator::Or: case BinaryOperator::Nand:
            case BinaryOperator::Nor: case BinaryOperator::Xor: case BinaryOperator::Xnor:
                return true;
            default:
                return false;
        }
    }

    inline bool isRelationalOperator(BinaryOperator op)
    {
        switch (op)
        {
            case BinaryOperator::Eq: case BinaryOperator::Neq: case BinaryOperator::Lt:
            case BinaryOperator::Le: case BinaryOperator::Gt:  case BinaryOperator::Ge:
                return true;
            default:
                return false;
        }
    }

    inline bool isMatchingOperator(BinaryOperator op)
    {
        switch (op)
        {
            case BinaryOperator::MatchEq: case BinaryOperator::MatchNeq: case BinaryOperator::MatchLt:
            case BinaryOperator::MatchLe: case BinaryOperator::MatchGt:  case BinaryOperator::MatchGe:
                return true;
            default:
                return false;
        }
    }

    inline bool isShiftOperator(BinaryOperator op)
    {
        switch (op)
        {
            case BinaryOperator::Sll: case BinaryOperator::Srl: case BinaryOperator::Sla:
            case BinaryOperator::Sra: case BinaryOperator::Rol: case BinaryOperator::Ror:
                return true;
            default:
                return false;
        }
    }

    inline bool isRangeOperator(BinaryOperator op)
    {
        return op == BinaryOperator::To || op == BinaryOperator::Downto;
    }

    /// The logical reduction (`and v`) of a binary logical operator, and its inverse.
    inline UnaryOperator reductionOf(BinaryOperator op)
    {
        switch (op)
        {
            case BinaryOperator::And:  return UnaryOperator::ReduceAnd;
            case BinaryOperator::Or:   return UnaryOperator::ReduceOr;
            case BinaryOperator::Nand: return UnaryOperator::ReduceNand;
            case BinaryOperator::Nor:  return UnaryOperator::ReduceNor;
            case BinaryOperator::Xor:  return UnaryOperator::ReduceXor;
            default:                   return UnaryOperator::ReduceXnor;
        }
    }

    // ---- Type predicates ------------------------------------------------------------------------

    inline bool isNumericVector(const SemanticType& type)
    {
        return isArray(type) && (type.info->family == VectorFamily::Unsigned || type.info->family == VectorFamily::Signed);
    }

    inline bool isPhysicalType(const SemanticType& type)
    {
        return type.info && type.info->cls == TypeClass::Physical;
    }

    /// A type whose values the logical operators (and, or, not, ...) accept, given the predefined boolean.
    inline bool isLogicalScalar(const SemanticType& type, const TypeInfo* boolean)
    {
        return type.info && type.info->cls == TypeClass::Enumeration && (type.info == boolean || type.info->isResolved);
    }

    inline bool isLogicalOperand(const SemanticType& type, const TypeInfo* boolean)
    {
        if (isLogicalScalar(type, boolean)) return true;
        return isOneDimensionalArray(type) && isLogicalScalar(type.info->element, boolean);
    }

    // ---- Messages -------------------------------------------------------------------------------

    /// `Operator '+' cannot be applied to 'a' and 'b'` with an optional explanation.
    inline std::string operatorProblem(const std::string& op, const SemanticType& left, const SemanticType& right,
                                       const std::string& reason = "")
    {
        return "Operator '" + op + "' cannot be applied to '" + describe(left) + "' and '" + describe(right) + "'"
             + (reason.empty() ? "" : ": " + reason);
    }

    inline std::string operatorProblem(const std::string& op, const SemanticType& operand, const std::string& reason = "")
    {
        return "Operator '" + op + "' cannot be applied to '" + describe(operand) + "'" + (reason.empty() ? "" : ": " + reason);
    }

    /// A one-dimensional array of `info` with `length` elements, `length-1 downto 0` (or `0 to length-1` when `ascending`);
    /// unconstrained when the length is unknown.
    inline SemanticType arrayOfLength(const TypeInfo& info, std::optional<int64_t> length, bool ascending = false)
    {
        SemanticType type;
        type.info = &info;
        if (length && *length > 0)
            type.dims = { ascending ? Bounds{ 0, *length - 1, true } : Bounds{ *length - 1, 0, false } };
        return type;
    }

    /// Result of applying an arithmetic operator to typed operands: the base type without subtype constraint.
    inline SemanticType baseResult(const SemanticType& type)
    {
        return isUniversal(type) ? type : typeOf(*type.info);
    }

} // namespace Pulse::Parser

#endif // PULSE_COMPILER_OPERATOR_HELPERS_H

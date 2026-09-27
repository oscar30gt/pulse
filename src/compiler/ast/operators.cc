#include "operators.h"

namespace Pulse::Parser
{
    const char* toString(BinaryOperator op)
    {
        switch (op)
        {
            case BinaryOperator::And:      return "and";
            case BinaryOperator::Or:       return "or";
            case BinaryOperator::Nand:     return "nand";
            case BinaryOperator::Nor:      return "nor";
            case BinaryOperator::Xor:      return "xor";
            case BinaryOperator::Xnor:     return "xnor";
            case BinaryOperator::Eq:       return "=";
            case BinaryOperator::Neq:      return "/=";
            case BinaryOperator::Lt:       return "<";
            case BinaryOperator::Le:       return "<=";
            case BinaryOperator::Gt:       return ">";
            case BinaryOperator::Ge:       return ">=";
            case BinaryOperator::MatchEq:  return "?=";
            case BinaryOperator::MatchNeq: return "?/=";
            case BinaryOperator::MatchLt:  return "?<";
            case BinaryOperator::MatchLe:  return "?<=";
            case BinaryOperator::MatchGt:  return "?>";
            case BinaryOperator::MatchGe:  return "?>=";
            case BinaryOperator::Sll:      return "sll";
            case BinaryOperator::Srl:      return "srl";
            case BinaryOperator::Sla:      return "sla";
            case BinaryOperator::Sra:      return "sra";
            case BinaryOperator::Rol:      return "rol";
            case BinaryOperator::Ror:      return "ror";
            case BinaryOperator::Add:      return "+";
            case BinaryOperator::Sub:      return "-";
            case BinaryOperator::Mul:      return "*";
            case BinaryOperator::Div:      return "/";
            case BinaryOperator::Mod:      return "mod";
            case BinaryOperator::Rem:      return "rem";
            case BinaryOperator::Pow:      return "**";
            case BinaryOperator::Concat:   return "&";
            case BinaryOperator::To:       return "to";
            case BinaryOperator::Downto:   return "downto";
        }
        return "";
    }

    const char* toString(UnaryOperator op)
    {
        switch (op)
        {
            case UnaryOperator::Plus:       return "+";
            case UnaryOperator::Minus:      return "-";
            case UnaryOperator::Abs:        return "abs";
            case UnaryOperator::Not:        return "not";
            case UnaryOperator::Condition:  return "??";
            case UnaryOperator::ReduceAnd:  return "and";
            case UnaryOperator::ReduceOr:   return "or";
            case UnaryOperator::ReduceNand: return "nand";
            case UnaryOperator::ReduceNor:  return "nor";
            case UnaryOperator::ReduceXor:  return "xor";
            case UnaryOperator::ReduceXnor: return "xnor";
        }
        return "";
    }

} // namespace Pulse::Parser

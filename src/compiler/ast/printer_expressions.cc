#include "printer.h"

#include <sstream>

namespace Pulse::Parser
{
    using namespace AstPrint;

    namespace
    {
        const char* objectClassName(ExternalObjectClass objectClass)
        {
            switch (objectClass)
            {
                case ExternalObjectClass::Constant: return "constant";
                case ExternalObjectClass::Signal:   return "signal";
                case ExternalObjectClass::Variable: return "variable";
            }
            return "";
        }

        std::string toText(double value)
        {
            std::ostringstream stream;
            stream << value;
            return stream.str();
        }
    } // anonymous namespace

    // ---- Names ----------------------------------------------------------------

    void SymbolExpr::print(int indent) const
    {
        printLine(indent, "SYMBOL", name);
    }

    void FieldAccessExpr::print(int indent) const
    {
        printLine(indent, "FIELD ACCESS", fieldName);
        printField(indent + 1, "target", target.get());
    }

    void FunctionCallExpr::print(int indent) const
    {
        printLine(indent, "CALL / INDEX");
        printField(indent + 1, "callee", callee.get());
        printList(indent + 1, "arguments", arguments);
    }

    void SignatureExpr::print(int indent) const
    {
        printLine(indent, "SIGNATURE");
        printList(indent + 1, "parameters", parameters);
        printOptionalField(indent + 1, "return", returnType.get());
    }

    void AttributeExpr::print(int indent) const
    {
        printLine(indent, "ATTRIBUTE", attributeName);
        printField(indent + 1, "prefix", prefix.get());
        if (signature)
            signature->print(indent + 1);
    }

    void QualifiedExpr::print(int indent) const
    {
        printLine(indent, "QUALIFIED");
        printField(indent + 1, "type mark", typeMark.get());
        printField(indent + 1, "operand", operand.get());
    }

    void NamedAssociationExpr::print(int indent) const
    {
        printLine(indent, "ASSOCIATION");
        printField(indent + 1, "formal", formal.get());
        printField(indent + 1, "actual", actual.get());
    }

    void OpenExpr::print(int indent) const
    {
        printLine(indent, "OPEN");
    }

    void ExternalNameExpr::print(int indent) const
    {
        printLine(indent, "EXTERNAL NAME", std::string(objectClassName(objectClass)) + " " + path);
        printField(indent + 1, "subtype", subtype.get());
    }

    // ---- Subtype indications --------------------------------------------------

    void TypeSpec::print(int indent) const
    {
        printLine(indent, "TYPE", typeName, TYPE);
        printOptionalField(indent + 1, "resolution", resolution.get());
        printList(indent + 1, "constraints", args);
        printOptionalField(indent + 1, "range", range.get());
    }

    void ElementResolutionExpr::print(int indent) const
    {
        printLine(indent, "ELEMENT RESOLUTION");
        printChild(resolution.get(), indent + 1);
    }

    // ---- Literals -------------------------------------------------------------

    void IntegerLiteralExpr::print(int indent) const
    {
        printLine(indent, "INTEGER", std::to_string(value), VALUE);
    }

    void DoubleLiteralExpr::print(int indent) const
    {
        printLine(indent, "REAL", toText(value), VALUE);
    }

    void PhysicalLiteralExpr::print(int indent) const
    {
        printLine(indent, "PHYSICAL", unit, TYPE);
        printChild(magnitude.get(), indent + 1);
    }

    void CharacterLiteralExpr::print(int indent) const
    {
        printLine(indent, "CHARACTER", value, VALUE);
    }

    void StringLiteralExpr::print(int indent) const
    {
        printLine(indent, "STRING", quoted(value), VALUE);
    }

    // ---- Choices and aggregates -----------------------------------------------

    void OthersExpr::print(int indent) const
    {
        printLine(indent, "OTHERS", "", VALUE);
    }

    void AllExpr::print(int indent) const
    {
        printLine(indent, "ALL", "", VALUE);
    }

    void ChoiceListExpr::print(int indent) const
    {
        printLine(indent, "CHOICES");
        for (const auto& alternative : alternatives)
            printChild(alternative.get(), indent + 1);
    }

    void AggregateExpr::print(int indent) const
    {
        printLine(indent, "AGGREGATE");
        for (const auto& element : elements)
            printChild(element.get(), indent + 1);
    }

    // ---- Operators ------------------------------------------------------------

    void BinaryOpExpr::print(int indent) const
    {
        printLine(indent, "BINARY OP", toString(op), VALUE);
        printField(indent + 1, "left", left.get());
        printField(indent + 1, "right", right.get());
    }

    void UnaryOpExpr::print(int indent) const
    {
        printLine(indent, "UNARY OP", toString(op), VALUE);
        printChild(operand.get(), indent + 1);
    }

    void WhenElseExpr::print(int indent) const
    {
        printLine(indent, "WHEN ELSE");
        printField(indent + 1, "value", trueValue.get());
        printField(indent + 1, "when", condition.get());
        printField(indent + 1, "else", falseValue.get());
    }

    void UnaffectedExpr::print(int indent) const
    {
        printLine(indent, "UNAFFECTED", "", VALUE);
    }

} // namespace Pulse::Parser

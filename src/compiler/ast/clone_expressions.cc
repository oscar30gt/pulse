#include "clone_helpers.h"

namespace Pulse::Parser
{
    // ---- Names ----------------------------------------------------------------

    std::unique_ptr<ASTNode> SymbolExpr::clone() const
    {
        auto copy = copyShell(*this);
        copy->name = name;
        return copy;
    }

    std::unique_ptr<ASTNode> FieldAccessExpr::clone() const
    {
        auto copy = copyShell(*this);
        copy->target = cloneOf(target);
        copy->fieldName = fieldName;
        return copy;
    }

    std::unique_ptr<ASTNode> FunctionCallExpr::clone() const
    {
        auto copy = copyShell(*this);
        copy->callee = cloneOf(callee);
        copy->arguments = cloneAll(arguments);
        return copy;
    }

    std::unique_ptr<ASTNode> SignatureExpr::clone() const
    {
        auto copy = copyShell(*this);
        copy->parameters = cloneAll(parameters);
        copy->returnType = cloneOf(returnType);
        return copy;
    }

    std::unique_ptr<ASTNode> AttributeExpr::clone() const
    {
        auto copy = copyShell(*this);
        copy->prefix = cloneOf(prefix);
        copy->signature = cloneOf(signature);
        copy->attributeName = attributeName;
        return copy;
    }

    std::unique_ptr<ASTNode> QualifiedExpr::clone() const
    {
        auto copy = copyShell(*this);
        copy->typeMark = cloneOf(typeMark);
        copy->operand = cloneOf(operand);
        return copy;
    }

    std::unique_ptr<ASTNode> NamedAssociationExpr::clone() const
    {
        auto copy = copyShell(*this);
        copy->formal = cloneOf(formal);
        copy->actual = cloneOf(actual);
        return copy;
    }

    std::unique_ptr<ASTNode> OpenExpr::clone() const
    {
        return copyShell(*this);
    }

    std::unique_ptr<ASTNode> ExternalNameExpr::clone() const
    {
        auto copy = copyShell(*this);
        copy->objectClass = objectClass;
        copy->path = path;
        copy->subtype = cloneOf(subtype);
        return copy;
    }

    std::unique_ptr<ASTNode> TypeSpec::clone() const
    {
        auto copy = copyShell(*this);
        copy->resolution = cloneOf(resolution);
        copy->typeName = typeName;
        copy->args = cloneAll(args);
        copy->range = cloneOf(range);
        return copy;
    }

    std::unique_ptr<ASTNode> ElementResolutionExpr::clone() const
    {
        auto copy = copyShell(*this);
        copy->resolution = cloneOf(resolution);
        return copy;
    }

    // ---- Literals -------------------------------------------------------------

    std::unique_ptr<ASTNode> IntegerLiteralExpr::clone() const
    {
        auto copy = copyShell(*this);
        copy->value = value;
        return copy;
    }

    std::unique_ptr<ASTNode> DoubleLiteralExpr::clone() const
    {
        auto copy = copyShell(*this);
        copy->value = value;
        return copy;
    }

    std::unique_ptr<ASTNode> PhysicalLiteralExpr::clone() const
    {
        auto copy = copyShell(*this);
        copy->magnitude = cloneOf(magnitude);
        copy->unit = unit;
        return copy;
    }

    std::unique_ptr<ASTNode> CharacterLiteralExpr::clone() const
    {
        auto copy = copyShell(*this);
        copy->value = value;
        return copy;
    }

    std::unique_ptr<ASTNode> StringLiteralExpr::clone() const
    {
        auto copy = copyShell(*this);
        copy->value = value;
        return copy;
    }

    // ---- Choices and aggregates -----------------------------------------------

    std::unique_ptr<ASTNode> OthersExpr::clone() const
    {
        return copyShell(*this);
    }

    std::unique_ptr<ASTNode> AllExpr::clone() const
    {
        return copyShell(*this);
    }

    std::unique_ptr<ASTNode> ChoiceListExpr::clone() const
    {
        auto copy = copyShell(*this);
        copy->alternatives = cloneAll(alternatives);
        return copy;
    }

    std::unique_ptr<ASTNode> AggregateExpr::clone() const
    {
        auto copy = copyShell(*this);
        copy->elements = cloneAll(elements);
        return copy;
    }

    // ---- Operators ------------------------------------------------------------

    std::unique_ptr<ASTNode> BinaryOpExpr::clone() const
    {
        auto copy = copyShell(*this);
        copy->op = op;
        copy->left = cloneOf(left);
        copy->right = cloneOf(right);
        return copy;
    }

    std::unique_ptr<ASTNode> UnaryOpExpr::clone() const
    {
        auto copy = copyShell(*this);
        copy->op = op;
        copy->operand = cloneOf(operand);
        return copy;
    }

    std::unique_ptr<ASTNode> WhenElseExpr::clone() const
    {
        auto copy = copyShell(*this);
        copy->trueValue = cloneOf(trueValue);
        copy->condition = cloneOf(condition);
        copy->falseValue = cloneOf(falseValue);
        return copy;
    }

    std::unique_ptr<ASTNode> UnaffectedExpr::clone() const
    {
        return copyShell(*this);
    }

} // namespace Pulse::Parser

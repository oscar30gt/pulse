#include "clone_helpers.h"

namespace Pulse::Parser
{
    // ---- Type definitions -----------------------------------------------------

    std::unique_ptr<ASTNode> NumericTypeDefinition::clone() const
    {
        auto copy = copyShell(*this);
        copy->range = cloneOf(range);
        return copy;
    }

    std::unique_ptr<ASTNode> EnumeratedTypeDefinition::clone() const
    {
        auto copy = copyShell(*this);
        copy->literals = literals;
        return copy;
    }

    std::unique_ptr<ASTNode> UnitDeclaration::clone() const
    {
        auto copy = copyShell(*this);
        copy->name = name;
        copy->multiplier = cloneOf(multiplier);
        copy->ofUnit = ofUnit;
        return copy;
    }

    std::unique_ptr<ASTNode> PhysicalTypeDefinition::clone() const
    {
        auto copy = copyShell(*this);
        copy->range = cloneOf(range);
        copy->units = cloneAll(units);
        return copy;
    }

    std::unique_ptr<ASTNode> ArrayTypeDefinition::clone() const
    {
        auto copy = copyShell(*this);
        copy->indexRanges = cloneAll(indexRanges);
        copy->elementType = cloneOf(elementType);
        return copy;
    }

    std::unique_ptr<ASTNode> UnconstrainedArrayTypeDefinition::clone() const
    {
        auto copy = copyShell(*this);
        copy->indexTypeMarks = indexTypeMarks;
        copy->elementType = cloneOf(elementType);
        return copy;
    }

    std::unique_ptr<ASTNode> RecordField::clone() const
    {
        auto copy = copyShell(*this);
        copy->name = name;
        copy->type = cloneOf(type);
        return copy;
    }

    std::unique_ptr<ASTNode> RecordTypeDefinition::clone() const
    {
        auto copy = copyShell(*this);
        copy->fields = cloneAll(fields);
        return copy;
    }

    // ---- Declarations ---------------------------------------------------------

    std::unique_ptr<ASTNode> TypeDeclaration::clone() const
    {
        auto copy = copyShell(*this);
        copy->name = name;
        copy->definition = cloneOf(definition);
        return copy;
    }

    std::unique_ptr<ASTNode> SubtypeDeclaration::clone() const
    {
        auto copy = copyShell(*this);
        copy->name = name;
        copy->baseType = cloneOf(baseType);
        return copy;
    }

    std::unique_ptr<ASTNode> SignalDeclaration::clone() const
    {
        auto copy = copyShell(*this);
        copy->name = name;
        copy->typeSpec = cloneOf(typeSpec);
        copy->initialValue = cloneOf(initialValue);
        return copy;
    }

    std::unique_ptr<ASTNode> ConstantDeclaration::clone() const
    {
        auto copy = copyShell(*this);
        copy->name = name;
        copy->typeSpec = cloneOf(typeSpec);
        copy->value = cloneOf(value);
        return copy;
    }

    std::unique_ptr<ASTNode> VariableDeclaration::clone() const
    {
        auto copy = copyShell(*this);
        copy->name = name;
        copy->typeSpec = cloneOf(typeSpec);
        copy->initialValue = cloneOf(initialValue);
        return copy;
    }

    // ---- Interface declarations -----------------------------------------------

    std::unique_ptr<ASTNode> GenericDeclaration::clone() const
    {
        auto copy = copyShell(*this);
        copy->name = name;
        copy->typeSpec = cloneOf(typeSpec);
        copy->defaultValue = cloneOf(defaultValue);
        return copy;
    }

    std::unique_ptr<ASTNode> PortDeclaration::clone() const
    {
        auto copy = copyShell(*this);
        copy->name = name;
        copy->typeSpec = cloneOf(typeSpec);
        copy->mode = mode;
        copy->defaultValue = cloneOf(defaultValue);
        return copy;
    }

    std::unique_ptr<ASTNode> ParameterDeclaration::clone() const
    {
        auto copy = copyShell(*this);
        copy->objectClass = objectClass;
        copy->name = name;
        copy->mode = mode;
        copy->typeSpec = cloneOf(typeSpec);
        copy->defaultValue = cloneOf(defaultValue);
        return copy;
    }

    std::unique_ptr<ASTNode> ComponentDeclaration::clone() const
    {
        auto copy = copyShell(*this);
        copy->name = name;
        copy->generics = cloneAll(generics);
        copy->ports = cloneAll(ports);
        return copy;
    }

    // ---- Context clauses ------------------------------------------------------

    std::unique_ptr<ASTNode> LibraryClause::clone() const
    {
        auto copy = copyShell(*this);
        copy->names = names;
        return copy;
    }

    std::unique_ptr<ASTNode> UseClause::clone() const
    {
        auto copy = copyShell(*this);
        copy->names = cloneAll(names);
        return copy;
    }

    // ---- Subprograms ----------------------------------------------------------

    std::unique_ptr<ASTNode> SubprogramSpec::clone() const
    {
        auto copy = copyShell(*this);
        copy->kind = kind;
        copy->impure = impure;
        copy->name = name;
        copy->parameters = cloneAll(parameters);
        copy->returnType = cloneOf(returnType);
        return copy;
    }

    std::unique_ptr<ASTNode> SubprogramDeclaration::clone() const
    {
        auto copy = copyShell(*this);
        copy->spec = cloneOf(spec);
        return copy;
    }

    std::unique_ptr<ASTNode> SubprogramBody::clone() const
    {
        auto copy = copyShell(*this);
        copy->spec = cloneOf(spec);
        copy->declarations = cloneAll(declarations);
        copy->body = cloneAll(body);
        return copy;
    }

    // ---- Aliases and attributes -----------------------------------------------

    std::unique_ptr<ASTNode> AliasDeclaration::clone() const
    {
        auto copy = copyShell(*this);
        copy->name = name;
        copy->subtype = cloneOf(subtype);
        copy->target = cloneOf(target);
        copy->signature = cloneOf(signature);
        return copy;
    }

    std::unique_ptr<ASTNode> AttributeDeclaration::clone() const
    {
        auto copy = copyShell(*this);
        copy->name = name;
        copy->typeMark = cloneOf(typeMark);
        return copy;
    }

    std::unique_ptr<ASTNode> AttributeSpecification::clone() const
    {
        auto copy = copyShell(*this);
        copy->attributeName = attributeName;
        copy->entities = cloneAll(entities);
        copy->entityClass = entityClass;
        copy->value = cloneOf(value);
        return copy;
    }

    // ---- Design units ---------------------------------------------------------

    std::unique_ptr<ASTNode> EntityDeclaration::clone() const
    {
        auto copy = copyShell(*this);
        copy->name = name;
        copy->generics = cloneAll(generics);
        copy->ports = cloneAll(ports);
        return copy;
    }

    std::unique_ptr<ASTNode> ArchitectureDeclaration::clone() const
    {
        auto copy = copyShell(*this);
        copy->entityName = entityName;
        copy->name = name;
        copy->declarations = cloneAll(declarations);
        copy->body = cloneAll(body);
        return copy;
    }

} // namespace Pulse::Parser

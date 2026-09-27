#include "printer.h"

namespace Pulse::Parser
{
    using namespace AstPrint;

    namespace
    {
        const char* parameterClassName(ParameterClass objectClass)
        {
            switch (objectClass)
            {
                case ParameterClass::Unspecified: return "";
                case ParameterClass::Constant:    return "constant";
                case ParameterClass::Signal:      return "signal";
                case ParameterClass::Variable:    return "variable";
            }
            return "";
        }

        /// Joins names on one line: "(a, b, '0')".
        std::string joinNames(const std::vector<std::string>& names)
        {
            std::string text = "(";
            for (size_t i = 0; i < names.size(); ++i)
                text += (i ? ", " : "") + names[i];
            return text + ")";
        }
    } // anonymous namespace

    const char* toString(PortMode mode)
    {
        switch (mode)
        {
            case PortMode::In:      return "in";
            case PortMode::Out:     return "out";
            case PortMode::InOut:   return "inout";
        }
        return "";
    }

    const char* toString(EntityClass entityClass)
    {
        switch (entityClass)
        {
            case EntityClass::Entity:        return "entity";
            case EntityClass::Architecture:  return "architecture";
            case EntityClass::Configuration: return "configuration";
            case EntityClass::Procedure:     return "procedure";
            case EntityClass::Function:      return "function";
            case EntityClass::Package:       return "package";
            case EntityClass::Type:          return "type";
            case EntityClass::Subtype:       return "subtype";
            case EntityClass::Constant:      return "constant";
            case EntityClass::Signal:        return "signal";
            case EntityClass::Variable:      return "variable";
            case EntityClass::Component:     return "component";
            case EntityClass::Label:         return "label";
            case EntityClass::Literal:       return "literal";
            case EntityClass::Units:         return "units";
            case EntityClass::Group:         return "group";
            case EntityClass::File:          return "file";
            case EntityClass::Property:      return "property";
            case EntityClass::Sequence:      return "sequence";
        }
        return "";
    }

    // ---- Type definitions -----------------------------------------------------

    void NumericTypeDefinition::print(int indent) const
    {
        printLine(indent, "NUMERIC TYPE");
        printField(indent + 1, "range", range.get());
    }

    void EnumeratedTypeDefinition::print(int indent) const
    {
        printLine(indent, "ENUMERATION", joinNames(literals), VALUE);
    }

    void UnitDeclaration::print(int indent) const
    {
        printLine(indent, "UNIT", name);
        if (!ofUnit.empty())
            printLine(indent + 1, "of unit", ofUnit);
        printOptionalField(indent + 1, "multiplier", multiplier.get());
    }

    void PhysicalTypeDefinition::print(int indent) const
    {
        printLine(indent, "PHYSICAL TYPE");
        printField(indent + 1, "range", range.get());
        printList(indent + 1, "units", units);
    }

    void ArrayTypeDefinition::print(int indent) const
    {
        printLine(indent, "ARRAY TYPE");
        printList(indent + 1, "index ranges", indexRanges);
        printField(indent + 1, "element type", elementType.get());
    }

    void UnconstrainedArrayTypeDefinition::print(int indent) const
    {
        printLine(indent, "UNCONSTRAINED ARRAY TYPE");
        for (const auto& mark : indexTypeMarks)
            printLine(indent + 1, "index type", mark, TYPE);
        printField(indent + 1, "element type", elementType.get());
    }

    void RecordField::print(int indent) const
    {
        printLine(indent, "FIELD", name);
        printChild(type.get(), indent + 1);
    }

    void RecordTypeDefinition::print(int indent) const
    {
        printLine(indent, "RECORD TYPE");
        printList(indent + 1, "fields", fields);
    }

    // ---- Declarations ---------------------------------------------------------

    void TypeDeclaration::print(int indent) const
    {
        printLine(indent, "TYPE DECLARATION", name);
        printChild(definition.get(), indent + 1);
    }

    void SubtypeDeclaration::print(int indent) const
    {
        printLine(indent, "SUBTYPE DECLARATION", name);
        printChild(baseType.get(), indent + 1);
    }

    void SignalDeclaration::print(int indent) const
    {
        printLine(indent, "SIGNAL DECLARATION", name);
        printField(indent + 1, "type", typeSpec.get());
        printOptionalField(indent + 1, "initial value", initialValue.get());
    }

    void ConstantDeclaration::print(int indent) const
    {
        printLine(indent, "CONSTANT DECLARATION", name);
        printField(indent + 1, "type", typeSpec.get());
        printField(indent + 1, "value", value.get());
    }

    void VariableDeclaration::print(int indent) const
    {
        printLine(indent, "VARIABLE DECLARATION", name);
        printField(indent + 1, "type", typeSpec.get());
        printOptionalField(indent + 1, "initial value", initialValue.get());
    }

    // ---- Interface declarations -----------------------------------------------

    void GenericDeclaration::print(int indent) const
    {
        printLine(indent, "GENERIC", name);
        printField(indent + 1, "type", typeSpec.get());
        printOptionalField(indent + 1, "default", defaultValue.get());
    }

    void PortDeclaration::print(int indent) const
    {
        printLine(indent, "PORT", name);
        printLine(indent + 1, "mode", toString(mode), TYPE);
        printField(indent + 1, "type", typeSpec.get());
        printOptionalField(indent + 1, "default", defaultValue.get());
    }

    void ParameterDeclaration::print(int indent) const
    {
        printLine(indent, "PARAMETER", name);
        if (objectClass != ParameterClass::Unspecified)
            printLine(indent + 1, "class", parameterClassName(objectClass), TYPE);
        printLine(indent + 1, "mode", toString(mode), TYPE);
        printField(indent + 1, "type", typeSpec.get());
        printOptionalField(indent + 1, "default", defaultValue.get());
    }

    void ComponentDeclaration::print(int indent) const
    {
        printLine(indent, "COMPONENT DECLARATION", name);
        printList(indent + 1, "generics", generics);
        printList(indent + 1, "ports", ports);
    }

    // ---- Context clauses ------------------------------------------------------

    void LibraryClause::print(int indent) const
    {
        printLine(indent, "LIBRARY", joinNames(names));
    }

    void UseClause::print(int indent) const
    {
        printLine(indent, "USE");
        for (const auto& name : names)
            printChild(name.get(), indent + 1);
    }

    // ---- Subprograms ----------------------------------------------------------

    void SubprogramSpec::print(int indent) const
    {
        const char* kindName = kind == SubprogramKind::Function ? (impure ? "IMPURE FUNCTION" : "FUNCTION") : "PROCEDURE";
        printLine(indent, kindName, name);
        printList(indent + 1, "parameters", parameters);
        printOptionalField(indent + 1, "return", returnType.get());
    }

    void SubprogramDeclaration::print(int indent) const
    {
        printLine(indent, "SUBPROGRAM DECLARATION");
        printChild(spec.get(), indent + 1);
    }

    void SubprogramBody::print(int indent) const
    {
        printLine(indent, "SUBPROGRAM BODY");
        printChild(spec.get(), indent + 1);
        printList(indent + 1, "declarations", declarations);
        printList(indent + 1, "body", body);
    }

    // ---- Aliases and attributes -----------------------------------------------

    void AliasDeclaration::print(int indent) const
    {
        printLine(indent, "ALIAS", name);
        printOptionalField(indent + 1, "subtype", subtype.get());
        printField(indent + 1, "target", target.get());
        if (signature)
            signature->print(indent + 1);
    }

    void AttributeDeclaration::print(int indent) const
    {
        printLine(indent, "ATTRIBUTE DECLARATION", name);
        printField(indent + 1, "type", typeMark.get());
    }

    void AttributeSpecification::print(int indent) const
    {
        printLine(indent, "ATTRIBUTE SPECIFICATION", attributeName);
        printLine(indent + 1, "class", toString(entityClass), TYPE);
        printList(indent + 1, "of", entities);
        printField(indent + 1, "value", value.get());
    }

    // ---- Design units ---------------------------------------------------------

    void EntityDeclaration::print(int indent) const
    {
        printLine(indent, "ENTITY", name);
        printList(indent + 1, "context", context);
        printList(indent + 1, "generics", generics);
        printList(indent + 1, "ports", ports);
    }

    void ArchitectureDeclaration::print(int indent) const
    {
        printLine(indent, "ARCHITECTURE", name);
        printLine(indent + 1, "of entity", entityName);
        printList(indent + 1, "context", context);
        printList(indent + 1, "declarations", declarations);
        printList(indent + 1, "body", body);
    }

} // namespace Pulse::Parser

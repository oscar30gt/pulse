#include "analyzer_internal.h"

#include <algorithm>

namespace Pulse::Parser
{
    /// An alias gives another name to a type, a subprogram, or an object (a whole one, an element, a slice or a field).
    void AnalyzerContext::declareAlias(const AliasDeclaration& decl)
    {
        const auto* name = dynamic_cast<const SymbolExpr*>(decl.target.get());
        const Symbol* target = name ? find(name->name) : nullptr;

        if (target && target->kind == SymbolKind::Type)
        {
            if (decl.subtype)
                fail("An alias of a type cannot have a subtype indication", *decl.subtype);
            if (decl.signature)
                fail("An alias of a type cannot have a signature", *decl.signature);

            Symbol alias;
            alias.kind = SymbolKind::Type;
            alias.type = target->type;      // the literals and units stay with the original type
            declare(decl.name, std::move(alias), decl);
            return;
        }

        if ((target && target->kind == SymbolKind::Subprogram) || decl.signature)
        {
            declareSubprogramAlias(decl);
            return;
        }

        declareObjectAlias(decl);
    }

    // ---- Subprograms ----------------------------------------------------------------------------

    void AnalyzerContext::declareSubprogramAlias(const AliasDeclaration& decl)
    {
        const auto* name = dynamic_cast<const SymbolExpr*>(decl.target.get());
        const Symbol* target = name ? find(name->name) : nullptr;
        if (!target || target->kind != SymbolKind::Subprogram)
            fail("An alias with a signature must name a subprogram", *decl.target);

        if (decl.subtype)
            fail("An alias of a subprogram cannot have a subtype indication", *decl.subtype);

        const auto overloads = visibleSubprograms(name->name);
        const SubprogramInfo* chosen = nullptr;

        if (decl.signature)
            chosen = &selectBySignature(overloads, *decl.signature, *decl.signature);
        else if (overloads.size() == 1)
            chosen = overloads.front();
        else
            fail("'" + name->name + "' is overloaded; the alias needs a signature to say which one it names", decl);

        if (decl.name.front() == '"')
        {
            SubprogramInfo renamed = *chosen;
            renamed.name = decl.name;
            checkOperatorProfile(renamed, decl);
        }

        Symbol* existing = findInCurrentScope(decl.name);
        if (existing && existing->kind != SymbolKind::Subprogram)
            fail("'" + decl.name + "' is already declared in this region", decl);

        if (!existing)
        {
            Symbol alias;
            alias.kind = SymbolKind::Subprogram;
            alias.overloads = { chosen };
            declare(decl.name, std::move(alias), decl);
            return;
        }

        for (const SubprogramInfo* overload : existing->overloads)
            if (sameProfile(*overload, *chosen))
                fail("The subprogram '" + describeProfile(*chosen) + "' is already declared under the name '" + decl.name + "' in this region", decl);

        existing->overloads.push_back(chosen);
    }

    // ---- Objects and values ---------------------------------------------------------------------

    void AnalyzerContext::declareObjectAlias(const AliasDeclaration& decl)
    {
        SemanticType subtype;
        if (decl.subtype)
            subtype = resolveTypeSpec(*decl.subtype);

        const Expression& target = *decl.target;
        const SemanticType targetType = exprType(target, decl.subtype ? &subtype : nullptr);
        const RootObject root = rootObject(target);

        Symbol alias;
        // exprType already refused everything that is not a value (types, components, labels, attributes), so a name with a root
        // is an object.
        if (root)
        {
            // The alias is the object itself: it shares its identity, mode, kind and place in the hierarchy.
            alias.kind = root.kind;
            alias.mode = root.mode;
            alias.objectId = root.objectId;
            alias.depth = root.depth;
            alias.isParameter = root.isParameter;
            alias.isPort = root.isPort;
            alias.partialAlias = !root.whole;
            if (root.kind == SymbolKind::Constant)
                alias.value = fold(target, &targetType);
        }
        else
        {
            // A literal, a unit or another constant expression: the alias is a constant of that value.
            if (!isStaticExpression(target))
                fail("An alias must name an object, a type or a subprogram; this expression is none of them", target);

            alias.kind = SymbolKind::Constant;
            alias.objectId = newObjectId();
            alias.value = fold(target, &targetType);
        }

        alias.type = targetType;

        if (decl.subtype)
        {
            if (subtype.info != targetType.info)
                fail("The subtype '" + describe(subtype) + "' of the alias does not match the type '" + describe(targetType) + "' of what it names", decl);

            const auto subtypeLength = staticLength(subtype);
            const auto targetLength = staticLength(targetType);
            if (subtypeLength && targetLength && *subtypeLength != *targetLength)
                fail("The subtype '" + describe(subtype) + "' of the alias has " + std::to_string(*subtypeLength) + " element(s), but '"
                     + describe(targetType) + "' has " + std::to_string(*targetLength), decl);

            // `alias a : bit_vector is v` (unconstrained) keeps the bounds of v; a constrained subtype re-indexes it.
            if (!(isArray(subtype) && !isConstrainedArray(subtype)))
                alias.type = subtype;
        }

        declare(decl.name, std::move(alias), decl);
    }

} // namespace Pulse::Parser

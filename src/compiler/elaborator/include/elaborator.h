#ifndef PULSE_VHDL_ELABORATOR_H
#define PULSE_VHDL_ELABORATOR_H

#include "analyzer.h"
#include "ast.h"
#include "blueprint.h"

#include <memory>
#include <string>
#include <vector>

namespace Pulse::Parser
{
    /// Exception type thrown when a design cannot be elaborated: an error only an instance can reveal (a generic without a
    /// value, widths that do not fit), or a construct the elaborator does not lower to logic components yet.
    class elaboration_error : public compiler_error
    {
    public:
        elaboration_error(const std::string& message, const SourceLocation& location)
            : compiler_error(message, location) { }
    };

    /// How std_logic values are represented in the simulated design.
    enum class LogicMode
    {
        /// `-Ologic`: std_logic is the engine's four-valued logic. '0' and 'L' are 0, '1' and 'H' are 1, 'Z' is Z and
        /// 'U', 'X', 'W' and '-' are X, so every std_logic operator maps onto a logic component.
        Logic,
        /// Nine-valued std_logic vectors, whose operators would run the IEEE function bodies. Not supported yet.
        Vector,
    };

    /// What to elaborate.
    struct ElaborationOptions
    {
        std::string topEntity = "top";      ///< Name of the top entity (lowercased)
        std::string topArchitecture;        ///< Architecture of the top entity; empty picks its most recently analyzed one
        LogicMode logic = LogicMode::Logic;
    };

    /// The blueprints of an elaborated design. Every distinct set of generic values of an entity gets a blueprint of its own,
    /// like a C++ template instantiation, and instances with the same values share it.
    struct ElaboratedDesign
    {
        std::vector<std::unique_ptr<Engine::Blueprint>> blueprints;    ///< Every blueprint, children before their parents
        Engine::Blueprint* top = nullptr;                               ///< The blueprint of the top entity
    };

    /// Elaborates a linked design (LRM 14): starting at the top entity, it picks each entity's architecture (the most
    /// recently analyzed one, LRM 7.3.3), gives every generic its value, and lowers each architecture to a Blueprint of logic
    /// components (gates, arithmetic, comparators, processes and nested subgraphs) that the engine can simulate.
    /// @param library The library the design files were analyzed into; the elaborator reads the types and names it resolved.
    /// @param linkedDesign The linked design. Read-only; it must outlive the returned design only while elaborating.
    /// @param options Top entity, its architecture and the logic representation.
    /// @throws elaboration_error when the design cannot be elaborated or uses a construct that is not supported yet.
    ElaboratedDesign elaborate(const DesignLibrary& library, const ASTRoot& linkedDesign, const ElaborationOptions& options);

} // namespace Pulse::Parser

#endif // PULSE_VHDL_ELABORATOR_H

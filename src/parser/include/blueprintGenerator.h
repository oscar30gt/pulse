#ifndef PULSE_VHDL_BLUEPRINT_GENERATOR_H
#define PULSE_VHDL_BLUEPRINT_GENERATOR_H

#include "Linker.h"
#include "blueprint.h"
#include <unordered_map>
#include <memory>
#include <string>

namespace Pulse::Parser
{
    /// Untility class that generates Blueprints from a LinkedDesign.
    /// A linked design is the final representation of a VHDL design before the whole design
    /// is converted into a instantiable blueprint representation.
    class BlueprintGenerator
    {
    public:
        BlueprintGenerator() = default;

        using EntityName = std::string;

        /// Converts a linked design into a set of blueprints. Each component architecture implementation
        /// is converted into its own blueprint.
        /// By default, the behavioral architecture is used to generate the blueprints. If a different architecture
        /// is desired, it can be specified by passing the architecture name as the second argument.
        std::unordered_map<EntityName, std::unique_ptr<Pulse::Engine::Blueprint>>
            generate(const LinkedDesign& design, std::string architectureName = "behavioral");
    };
} // namespace Pulse::Parser

#endif // PULSE_VHDL_BLUEPRINT_GENERATOR_H

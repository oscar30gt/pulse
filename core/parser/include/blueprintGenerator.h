#ifndef PULSE_VHDL_BLUEPRINT_GENERATOR_H
#define PULSE_VHDL_BLUEPRINT_GENERATOR_H

#include "Linker.h"
#include "blueprint.h"
#include <unordered_map>
#include <memory>
#include <string>

namespace Pulse::Parser::VHDL
{
    class BlueprintGenerator
    {
    public:
        BlueprintGenerator() = default;

        // Converts a LinkedDesign into a map of Blueprints, keyed by architecture name.
        std::unordered_map<std::string, std::unique_ptr<Pulse::Parser::Blueprint>> 
        generate(const LinkedDesign& design);

    private:
        int m_tempWireCounter = 0;
        int m_compCounter = 0;

        std::string genTempWire(Pulse::Parser::Blueprint& bp, bitWidth_t width = 1);
        std::string genCompName(const std::string& prefix);

        // Recursively flattens an AST expression into hardware components.
        // Returns the name of the wire containing the expression's result.
        std::string buildExpression(const ASTNode* node, Pulse::Parser::Blueprint& bp, const std::string& targetWire);

        // Helper to extract an integer literal for slicing/splitters
        bitWidth_t evaluateStaticInteger(const ASTNode* node);
    };
} // namespace Pulse::Parser::VHDL

#endif // PULSE_VHDL_BLUEPRINT_GENERATOR_H
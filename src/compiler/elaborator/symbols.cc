#include "elaborator_internal.h"

namespace Pulse::Parser
{
    Engine::SignalSymbol UnitElaborator::symbolOf(const Layout& layout) const
    {
        Engine::SignalSymbol symbol;
        switch (layout.kind)
        {
            case Layout::Kind::Logic:
                symbol.format = Engine::DisplayFormat::Bit;
                break;
            case Layout::Kind::Boolean:
                symbol.format = Engine::DisplayFormat::Boolean;
                break;
            case Layout::Kind::Enumeration:
                symbol.format = Engine::DisplayFormat::Enumeration;
                symbol.literals = layout.type->literals;
                break;
            case Layout::Kind::Integer:
                symbol.format = layout.isSigned ? Engine::DisplayFormat::SignedInteger : Engine::DisplayFormat::UnsignedInteger;
                break;
            case Layout::Kind::Vector:
                symbol.format = Engine::DisplayFormat::Hex;
                symbol.left = layout.left;
                symbol.ascending = layout.ascending;
                break;
        }
        return symbol;
    }

    /// The ports and signals of the unit, as the waveform shows them (variables and hidden wires are not shown).
    void UnitElaborator::fillSymbols()
    {
        for (const auto& [declaration, object] : m_objects)
            if (object.objectClass != ObjectWire::Class::Variable)
                m_bp.symbols.signals[object.wire] = symbolOf(object.layout);
    }

} // namespace Pulse::Parser

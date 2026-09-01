#include "constant.h"

#include <stdexcept>

namespace Pulse::Engine
{
    Constant::Constant(Wire* out, LogicVector state)
        : Component({}, { {"out", out} }),
        m_out(out->width())
    { 
        try
        {
            m_out.drive(state);
            m_out.addTarget(out);
        }
        catch (const bit_width_mismatch& e)
        {
            throw bit_width_mismatch("Constant construction failed: " + std::string(e.what()));
        }
    }

    Constant::~Constant() = default;
}
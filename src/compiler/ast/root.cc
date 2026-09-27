#include "clone_helpers.h"

namespace Pulse::Parser
{
    std::unique_ptr<ASTNode> ASTRoot::clone() const
    {
        auto copy = copyShell(*this);
        copy->children = cloneAll(children);
        return copy;
    }

} // namespace Pulse::Parser

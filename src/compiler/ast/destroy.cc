#include "node.h"

namespace Pulse::Parser
{
    /// Takes a chain apart from its head: each link is detached from the next before it is destroyed, so no destructor
    /// ever finds a long chain below it.
    void unlinkSpine(ExpressionPtr& head)
    {
        ExpressionPtr current = std::move(head);

        while (current)
        {
            ExpressionPtr* link = current->spineChild();
            if (!link || !*link)
                return;

            // `current` is destroyed by the assignment; its spine child is already empty, so its destructor finds nothing to do.
            ExpressionPtr next = std::move(*link);
            current = std::move(next);
        }
    }

} // namespace Pulse::Parser

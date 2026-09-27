#ifndef PULSE_COMPILER_SUBPROGRAM_INFO_H
#define PULSE_COMPILER_SUBPROGRAM_INFO_H

#include "analyzer_scope.h"

#include <string>
#include <vector>

namespace Pulse::Parser
{
    /// A declared function or procedure: its profile (name, parameters, result) plus what its body does.
    /// Two subprograms are homographs, and cannot share a region, when their parameter and result base types agree.
    struct SubprogramInfo
    {
        std::string name;                       ///< Identifier, or an operator symbol with its quotes ("\"and\"")
        bool isFunction = false;
        bool impure = false;                    ///< `impure function`; procedures are always allowed to be impure
        std::vector<FormalInfo> parameters;
        SemanticType returnType;                ///< Functions only
        SourceLocation location;                ///< Where it was first declared
        /// A predefined IEEE subprogram of the prelude (std_logic_1164, numeric_std): it has no body in VHDL, the elaborator
        /// implements it natively.
        bool builtin = false;
        /// The declaration, or the body when it was declared by its body.
        const Declaration* declaration = nullptr;

        bool hasBody = false;                   ///< A body has been analyzed for it
        bool containsWait = false;              ///< Its body has a wait statement
        bool assignsSignals = false;            ///< Its body assigns a signal
        std::vector<const SubprogramInfo*> callees;   ///< Subprograms its body calls (checked once all bodies are known)
        std::vector<SourceLocation> calleeLocations;  ///< Where each callee is called, parallel to `callees`
    };

} // namespace Pulse::Parser

#endif // PULSE_COMPILER_SUBPROGRAM_INFO_H

#ifndef PULSE_VHDL_LINKER_H
#define PULSE_VHDL_LINKER_H

#include "ast.h"

namespace Pulse::Parser
{
    /// Exception type thrown when an error occurs during the linking process of the AST.
    class ast_link_error : public ast_error
    {
        SourceLocation m_location;

    public:
        ast_link_error(const std::string& message, const SourceLocation& location)
            : ast_error(message, location)
        { }
    };

    // --------------------------------------------------------------------------------------------

    class Linker
    {
        std::vector<ASTRoot> m_roots; /// List of AST roots to be linked together. 

    public:
        explicit Linker();

        /// Adds an AST root to the linker for processing.
        /// @param root Pointer to the AST root to be added. The linker will take ownership
        ///             and roots will be moved so they will no longer be valid after this call.
        void addAST(ASTRoot&& root);

        /// Links all added AST roots into a single design representation.
        /// @returns A new ASTRoot containing the linked design.
        ASTRoot link();
    };

} // namespace Pulse::Parser

#endif // PULSE_VHDL_LINKER_H
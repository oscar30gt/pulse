#ifndef PULSE_VHDL_LINKER_H
#define PULSE_VHDL_LINKER_H

#include "ast.h"

namespace Pulse::Parser
{
    /// Exception type thrown when an error occurs during the linking process of the AST.
    class ast_link_error : public compiler_error
    {
    public:
        ast_link_error(const std::string& message, const SourceLocation& location)
            : compiler_error(message, location)
        { }
    };

    // --------------------------------------------------------------------------------------------

    /// Merges the ASTs of several source files into one design.
    ///
    /// Each file is tokenized and parsed on its own, so an architecture can refer to an entity that lives in another
    /// file. The linker takes every parsed file (addAST), then link() produces a single ASTRoot in which all entities come
    /// first and all architectures after them, ready for the analyzer, which resolves the references between them.
    /// Linking only checks name collisions across files; it does not type check anything.
    class Linker
    {
        /// AST roots added so far, in the order they were added.
        std::vector<ASTRoot> m_roots; /// List of AST roots to be linked together. 

    public:
        /// Creates an empty linker.
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
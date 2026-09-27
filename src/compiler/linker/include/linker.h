#ifndef PULSE_VHDL_LINKER_H
#define PULSE_VHDL_LINKER_H

#include "analyzer.h"
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

        const char* stage() const override { return "link"; }
    };

    // --------------------------------------------------------------------------------------------

    /// Links the analyzed files of a design into one design.
    ///
    /// Every file is analyzed into a DesignLibrary before it is linked, so each architecture has already found its entity,
    /// wherever it was declared. What analysis leaves open is the entity a component instance stands for: the linker binds
    /// every instance to the entity with the same name as its component (the default binding of LRM 7.3.3) and checks that
    /// the component fits that entity. Every generic and port of the component must exist in the entity with the same type,
    /// every port with a mode the association allows, and whatever the component leaves out must have a default, unless it
    /// is an output or inout port, which simply stays open. link() then produces a single ASTRoot in which all entities come
    /// first and all architectures after them.
    class Linker
    {
        /// Library the files were analyzed into: it knows the component of every instance and the type of every generic
        /// and port.
        const DesignLibrary& m_library;
        /// AST roots added so far, in the order they were added.
        std::vector<ASTRoot> m_roots; /// List of AST roots to be linked together.

    public:
        /// Creates a linker for design files analyzed into `library`.
        explicit Linker(const DesignLibrary& library);

        /// Adds an AST root to the linker for processing.
        /// @param root Root of a file analyzed into the linker's library. The linker will take ownership
        ///             and roots will be moved so they will no longer be valid after this call.
        void addAST(ASTRoot&& root);

        /// Binds every component instance of the added roots to its entity, then links the roots into a single design
        /// representation.
        /// @returns A new ASTRoot containing the linked design.
        /// @throws ast_link_error if an instance cannot be bound to an entity; the added roots are then left untouched.
        ASTRoot link();
    };

} // namespace Pulse::Parser

#endif // PULSE_VHDL_LINKER_H

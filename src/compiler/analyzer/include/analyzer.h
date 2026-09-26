#ifndef PULSE_VHDL_ANALYZER_H
#define PULSE_VHDL_ANALYZER_H

#include "ast.h"
#include "semantic_type.h"

#include <cstddef>
#include <memory>
#include <vector>

namespace Pulse::Parser
{
    /// Exception type thrown when an error occurs during semantic analysis of the AST.
    class ast_semantic_error : public compiler_error
    {
    public:
        ast_semantic_error(const std::string& message, const SourceLocation& location)
            : compiler_error(message, location) { }
    };

    // --------------------------------------------------------------------------------------------

    class AnalyzerContext;

    /// The working library (`work`) of a design: the design units analyzed so far, and what analysis resolved in them
    /// that the linker needs to bind component instances to entities.
    ///
    /// Design files are analyzed into it one at a time, the design units of each file in textual order (LRM 13.1). An
    /// architecture needs its entity to be in the library already (LRM 13.5), analyzed from the same file or from an
    /// earlier one, so an architecture can live in another file than its entity. Names are unique: one entity per name
    /// and one architecture per name and entity. A unit enters the library only once its analysis succeeds.
    ///
    /// The library refers to the trees it analyzed without copying them, so they must outlive it.
    class DesignLibrary
    {
        /// The analysis state shared by every file: scopes, types, entities, and what the linker asks for.
        std::unique_ptr<AnalyzerContext> m_context;

    public:
        /// Creates a library that only holds the predefined types.
        DesignLibrary();
        ~DesignLibrary();
        DesignLibrary(const DesignLibrary&) = delete;
        DesignLibrary& operator=(const DesignLibrary&) = delete;

        /// Analyzes the design units of one design file into the library. This includes type checking, scope resolution
        /// and any other errors that were not caught during the AST construction phase.
        /// @param root The file to analyze. Read-only; the AST is not modified during analysis.
        /// @throws ast_semantic_error on the first violation found.
        void analyze(const ASTRoot& root);

        /// The component an analyzed component instantiation instantiates.
        /// @returns The component declaration, or nullptr when the instance was not analyzed into this library.
        const ComponentDeclaration* componentOf(const ComponentInstantiation& instance) const;

        /// The type analysis resolved for a generic or a port of an analyzed entity or component.
        /// @returns The type, or nullptr when the declaration was not analyzed into this library.
        const SemanticType* interfaceType(const Declaration& genericOrPort) const;
    };

    /// The order in which a set of design files is analyzed, as a make-like driver picks it: every file comes after the
    /// other files that declare the entities of its architectures, and files that do not depend on each other keep their
    /// order. Inside a file, units are analyzed in textual order, so an entity must come before its architectures there.
    /// @param files The parsed design files. Read-only.
    /// @returns Indices into `files`, in the order they must be analyzed.
    /// @throws ast_semantic_error when files need entities of each other, so that none of them can be analyzed first.
    std::vector<size_t> analysisOrder(const std::vector<ASTRoot>& files);

    /// Analyzes a design that fits in one file, in a library of its own.
    /// @param root The file to analyze. Read-only; the AST is not modified during analysis.
    /// @throws ast_semantic_error if any semantic errors are found during analysis.
    void analyzeAST(const ASTRoot& root);

} // namespace Pulse::Parser

#endif // PULSE_VHDL_ANALYZER_H

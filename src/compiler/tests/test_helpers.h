#ifndef PULSE_COMPILER_TESTS_HELPERS_H
#define PULSE_COMPILER_TESTS_HELPERS_H

// Shared helpers for the compiler test suites. Everything lives in namespace TestUtil so it
// never clashes with the small per-file helpers older suites still define.

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "analyzer.h"
#include "elaborator.h"
#include "linker.h"
#include "subgraph.h"
#include "parser.h"
#include "tokenizer.h"

namespace TestUtil
{
    using namespace Pulse::Parser;

    /// Tokenizes and parses a VHDL source string, as the source file `file` of a design.
    inline ASTRoot parseSource(const std::string& source, size_t file = noFile)
    {
        Tokenizer tokenizer(source, file);
        return VHDLtoAST(tokenizer);
    }

    /// The first architecture of a parsed root (fails the test if there is none).
    inline const ArchitectureDeclaration* firstArchitecture(const ASTRoot& root)
    {
        for (const auto& child : root.children)
            if (auto* arch = dynamic_cast<const ArchitectureDeclaration*>(child.get()))
                return arch;

        ADD_FAILURE() << "No architecture found in parsed AST";
        return nullptr;
    }

    /// Every declaration of the given node type from a declarative part, in source order.
    template <typename T>
    std::vector<const T*> declaredAs(const std::vector<DeclarationPtr>& declarations)
    {
        std::vector<const T*> result;
        for (const auto& decl : declarations)
            if (auto* typed = dynamic_cast<const T*>(decl.get()))
                result.push_back(typed);
        return result;
    }

    template <typename T>
    const T* nodeAs(const ASTNode* node)
    {
        return dynamic_cast<const T*>(node);
    }

    /// Message of the compiler_error thrown while parsing `source`, or "<no error>".
    inline std::string parseErrorMessage(const std::string& source)
    {
        try
        {
            parseSource(source);
        }
        catch (const compiler_error& e)
        {
            return e.what();
        }
        return "<no error>";
    }

    /// Parses and analyzes a source string (throws on any diagnostic).
    inline void analyzeSource(const std::string& source)
    {
        ASTRoot root = parseSource(source);
        analyzeAST(root);
    }

    /// Message of the diagnostic raised while analyzing `source`, or "<no error>". Parse errors count too.
    inline std::string analysisError(const std::string& source)
    {
        try
        {
            analyzeSource(source);
        }
        catch (const compiler_error& e)
        {
            return e.what();
        }
        return "<no error>";
    }

    /// Parses every source as a design file of its own (its index in `sources` is its file index) and analyzes the files
    /// into `library`, in the order analysisOrder() gives. Returns the parsed files: the library refers to them, so they must
    /// outlive it.
    inline std::vector<ASTRoot> analyzeFiles(const std::vector<std::string>& sources, DesignLibrary& library)
    {
        std::vector<ASTRoot> files;
        for (size_t index = 0; index < sources.size(); ++index)
            files.push_back(parseSource(sources[index], index));

        for (size_t index : analysisOrder(files))
            library.analyze(files[index]);
        return files;
    }

    /// Compiles design files like the compiler does: parses each, analyzes them in dependency order, then links them.
    inline ASTRoot compileFiles(const std::vector<std::string>& sources)
    {
        DesignLibrary library;
        std::vector<ASTRoot> files = analyzeFiles(sources, library);

        Linker linker(library);
        for (ASTRoot& file : files)
            linker.addAST(std::move(file));
        return linker.link();
    }

    /// Message of the diagnostic raised while compiling `sources` (parsing, analysis or linking), or "<no error>".
    inline std::string compileError(const std::vector<std::string>& sources)
    {
        try
        {
            compileFiles(sources);
        }
        catch (const compiler_error& e)
        {
            return e.what();
        }
        return "<no error>";
    }

    /// A design compiled like the compiler does it (parse, analyze, link), elaborated from its top entity and built into a
    /// root subgraph ready to simulate. Every stage keeps what the next one refers to, so the object is not copied or moved.
    struct Simulation
    {
        DesignLibrary library;
        ASTRoot design;
        ElaboratedDesign elaborated;
        std::unique_ptr<Pulse::Engine::Subgraph> graph;
        uint64_t ticks = 0;

        Simulation(const std::vector<std::string>& sources, const std::string& top = "top", const std::string& architecture = "")
        {
            std::vector<ASTRoot> files = analyzeFiles(sources, library);
            Linker linker(library);
            for (ASTRoot& file : files)
                linker.addAST(std::move(file));
            design = linker.link();

            ElaborationOptions options;
            options.topEntity = top;
            options.topArchitecture = architecture;
            elaborated = elaborate(library, design, options);
            graph = std::make_unique<Pulse::Engine::Subgraph>(*elaborated.top);
        }

        Simulation(const Simulation&) = delete;
        Simulation& operator=(const Simulation&) = delete;

        /// Runs the design for `count` ticks (femtoseconds).
        void run(uint64_t count)
        {
            for (uint64_t i = 0; i < count; ++i, ++ticks)
                graph->tick();
        }

        /// The value of a signal or port, by path: "s" in the top, "u1.s" in instance u1 of the top, and so on.
        Pulse::Engine::LogicVector value(const std::string& path) const
        {
            Pulse::Engine::SubgraphSnapshot snapshot = graph->takeSnapshot();
            const Pulse::Engine::SubgraphSnapshot* level = &snapshot;

            std::string rest = path;
            for (size_t dot = rest.find('.'); dot != std::string::npos; dot = rest.find('.'))
            {
                auto child = level->subgraphs.find(rest.substr(0, dot));
                if (child == level->subgraphs.end())
                    throw std::runtime_error("no instance '" + rest.substr(0, dot) + "' on the path " + path);
                level = &child->second;
                rest = rest.substr(dot + 1);
            }

            for (const auto* signals : { &level->inputs, &level->outputs, &level->wires })
                if (auto found = signals->find(rest); found != signals->end())
                    return found->second.second;
            throw std::runtime_error("no signal '" + rest + "' on the path " + path);
        }

        /// The value of a signal as an unsigned number (it must hold no X or Z).
        uint64_t number(const std::string& path) const
        {
            return static_cast<uint64_t>(value(path));
        }

        /// The value of a signal as a string of 0, 1, X and Z, `width` characters long.
        std::string bits(const std::string& path, uint8_t width) const
        {
            return value(path).str(width);
        }
    };

    /// Message of the diagnostic raised while compiling and elaborating `sources` from `top`, or "<no error>".
    inline std::string elaborationError(const std::vector<std::string>& sources, const std::string& top = "top")
    {
        try
        {
            Simulation simulation(sources, top);
        }
        catch (const compiler_error& e)
        {
            return e.what();
        }
        return "<no error>";
    }

    /// Renders a list of expressions separated by `separator`.
    template <typename List, typename Render>
    std::string renderList(const List& items, const char* separator, Render renderOne)
    {
        std::string text;
        for (size_t i = 0; i < items.size(); ++i)
            text += (i ? separator : "") + renderOne(items[i].get());
        return text;
    }

    /// Fully parenthesized one-line rendering of an expression, so precedence and associativity
    /// can be asserted as plain strings: `a + b * c` renders as `(a + (b * c))`. Calls and indexes
    /// render with brackets (`f[x]`), aggregates with braces (`{a, 0 => b}`).
    inline std::string render(const Expression* expr)
    {
        auto one = [](const Expression* e) { return render(e); };
        if (!expr) return "<null>";
        if (auto* n = dynamic_cast<const SymbolExpr*>(expr)) return n->name;
        if (auto* n = dynamic_cast<const IntegerLiteralExpr*>(expr)) return std::to_string(n->value);
        if (auto* n = dynamic_cast<const DoubleLiteralExpr*>(expr)) return std::to_string(n->value);
        if (auto* n = dynamic_cast<const CharacterLiteralExpr*>(expr)) return n->value;
        if (auto* n = dynamic_cast<const StringLiteralExpr*>(expr)) return "\"" + n->value + "\"";
        if (auto* n = dynamic_cast<const PhysicalLiteralExpr*>(expr))
            return render(n->magnitude.get()) + " " + n->unit;
        if (auto* n = dynamic_cast<const UnaryOpExpr*>(expr))
            return "(" + std::string(toString(n->op)) + " " + render(n->operand.get()) + ")";
        if (auto* n = dynamic_cast<const BinaryOpExpr*>(expr))
            return "(" + render(n->left.get()) + " " + toString(n->op) + " " + render(n->right.get()) + ")";
        if (auto* n = dynamic_cast<const FieldAccessExpr*>(expr)) return render(n->target.get()) + "." + n->fieldName;
        if (auto* n = dynamic_cast<const AttributeExpr*>(expr)) return render(n->prefix.get()) + "'" + n->attributeName;
        if (auto* n = dynamic_cast<const FunctionCallExpr*>(expr))
            return render(n->callee.get()) + "[" + renderList(n->arguments, ", ", one) + "]";
        if (auto* n = dynamic_cast<const WhenElseExpr*>(expr))
            return "(" + render(n->trueValue.get()) + " when " + render(n->condition.get()) + " else " + render(n->falseValue.get()) + ")";
        if (auto* n = dynamic_cast<const AggregateExpr*>(expr)) return "{" + renderList(n->elements, ", ", one) + "}";
        if (auto* n = dynamic_cast<const NamedAssociationExpr*>(expr))
            return render(n->formal.get()) + " => " + render(n->actual.get());
        if (auto* n = dynamic_cast<const ChoiceListExpr*>(expr)) return renderList(n->alternatives, " | ", one);
        if (auto* n = dynamic_cast<const QualifiedExpr*>(expr))
            return render(n->typeMark.get()) + "'(" + render(n->operand.get()) + ")";
        if (auto* n = dynamic_cast<const TypeSpec*>(expr))
        {
            std::string text = n->resolution ? render(n->resolution.get()) + " " : "";
            text += n->typeName;
            if (!n->args.empty()) text += "(" + renderList(n->args, ", ", one) + ")";
            if (n->range) text += " range " + render(n->range.get());
            return text;
        }
        if (auto* n = dynamic_cast<const ElementResolutionExpr*>(expr)) return "(" + render(n->resolution.get()) + ")";
        if (dynamic_cast<const OthersExpr*>(expr)) return "others";
        if (dynamic_cast<const OpenExpr*>(expr)) return "open";
        if (dynamic_cast<const UnaffectedExpr*>(expr)) return "unaffected";
        return "<?>";
    }

    /// Parses `target <= <expression>;` in an architecture and renders the assigned value.
    inline std::string renderValue(const std::string& expression)
    {
        ASTRoot root = parseSource("entity t is end t;\narchitecture a of t is\nbegin\ny <= " + expression + ";\nend a;\n");
        auto* assignment = dynamic_cast<const SignalAssignment*>(firstArchitecture(root)->body.at(0).get());
        return render(assignment->value.get());
    }

    /// Declarations of `character` and `string`. The predefined environment of the analyzer does not include them, so a test
    /// that reports or asserts declares them itself (character literals are enumeration literals of a type).
    inline std::string stringTypes()
    {
        std::string literals;
        for (int c = 32; c < 127; ++c)
            literals += std::string(literals.empty() ? "" : ", ") + "'" + static_cast<char>(c) + "'";

        return "type character is (" + literals + ");\ntype string is array (positive range <>) of character;\n";
    }

    /// Wraps declarations/statements into a minimal entity + architecture.
    inline std::string inArchitecture(const std::string& declarations, const std::string& body = "")
    {
        return "entity t is end t;\narchitecture a of t is\n" + declarations + "\nbegin\n" + body + "\nend a;\n";
    }

    /// Wraps process body text into a minimal design: `process <declarations> begin <statements> end process;`.
    inline std::string inProcess(const std::string& statements, const std::string& declarations = "")
    {
        return inArchitecture("", "process\n" + declarations + "\nbegin\n" + statements + "\nend process;");
    }

} // namespace TestUtil

#endif // PULSE_COMPILER_TESTS_HELPERS_H

#include "analyzer_internal.h"

#include <unordered_map>

namespace Pulse::Parser
{
    // ---- Design library -------------------------------------------------------------------------

    DesignLibrary::DesignLibrary()
        : m_context(std::make_unique<AnalyzerContext>())
    { }

    DesignLibrary::~DesignLibrary() = default;

    void DesignLibrary::analyze(const ASTRoot& root)
    {
        m_context->analyze(root);
    }

    const ComponentDeclaration* DesignLibrary::componentOf(const ComponentInstantiation& instance) const
    {
        return m_context->componentOf(instance);
    }

    const SemanticType* DesignLibrary::interfaceType(const Declaration& genericOrPort) const
    {
        return m_context->interfaceType(genericOrPort);
    }

    void analyzeAST(const ASTRoot& root)
    {
        DesignLibrary library;
        library.analyze(root);
    }

    // ---- Analysis order -------------------------------------------------------------------------

    namespace
    {
        /// A file that must be analyzed before another one, and the architecture of the other one that needs it.
        struct Dependency
        {
            size_t file = 0;
            const ArchitectureDeclaration* architecture = nullptr;
        };

        /// For each file, the other files that declare the entities of its architectures, in the order the architectures
        /// appear. An entity is looked up in the first file that declares it; a second declaration is reported by the analysis,
        /// and so is an architecture whose entity no file declares.
        std::vector<std::vector<Dependency>> fileDependencies(const std::vector<ASTRoot>& files)
        {
            std::unordered_map<std::string, size_t> entityFiles;
            for (size_t file = 0; file < files.size(); ++file)
                for (const auto& unit : files[file].children)
                    if (auto* entity = dynamic_cast<const EntityDeclaration*>(unit.get()))
                        entityFiles.emplace(entity->name, file);

            std::vector<std::vector<Dependency>> dependencies(files.size());
            for (size_t file = 0; file < files.size(); ++file)
            {
                for (const auto& unit : files[file].children)
                {
                    auto* architecture = dynamic_cast<const ArchitectureDeclaration*>(unit.get());
                    if (!architecture)
                        continue;

                    auto owner = entityFiles.find(architecture->entityName);
                    if (owner != entityFiles.end() && owner->second != file)
                        dependencies[file].push_back({ owner->second, architecture });
                }
            }
            return dependencies;
        }
    } // anonymous namespace

    /// A depth-first placement: a file is placed once every file it depends on is, and files are visited in their given
    /// order, so files that do not depend on each other keep it. The dependencies are walked with an explicit stack, so
    /// however long a chain of files is, it cannot exhaust the call stack.
    std::vector<size_t> analysisOrder(const std::vector<ASTRoot>& files)
    {
        const std::vector<std::vector<Dependency>> dependencies = fileDependencies(files);

        enum class State { Unplaced, Placing, Placed };
        std::vector<State> states(files.size(), State::Unplaced);
        std::vector<size_t> order;
        order.reserve(files.size());

        /// A file being placed and the next of its dependencies to look at.
        struct Frame
        {
            size_t file = 0;
            size_t next = 0;
        };
        std::vector<Frame> stack;

        for (size_t first = 0; first < files.size(); ++first)
        {
            if (states[first] != State::Unplaced)
                continue;

            states[first] = State::Placing;
            stack.push_back({ first, 0 });

            while (!stack.empty())
            {
                Frame& frame = stack.back();
                if (frame.next == dependencies[frame.file].size())
                {
                    states[frame.file] = State::Placed;
                    order.push_back(frame.file);
                    stack.pop_back();
                    continue;
                }

                const Dependency& dependency = dependencies[frame.file][frame.next++];
                if (states[dependency.file] == State::Placing)
                    throw ast_semantic_error("Architecture '" + dependency.architecture->name + "' needs the entity '"
                                             + dependency.architecture->entityName + "' of another file, which itself depends on "
                                             "this file, so neither file can be analyzed first", dependency.architecture->source);

                if (states[dependency.file] == State::Unplaced)
                {
                    states[dependency.file] = State::Placing;
                    stack.push_back({ dependency.file, 0 });
                }
            }
        }
        return order;
    }

} // namespace Pulse::Parser

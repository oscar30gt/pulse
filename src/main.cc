///
/// Pulse Simulator
/// Óscar Grimal Torres
///
/// Pulse is a multi-platform digital logic simulation engine for VHDL made with C++.
/// It transforms VHDL source code into a logic components simulation model that can be simulated and debugged.
///
/// Usage: ./pulse <project_path> [options]
///

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "analyzer.h"
#include "ast.h"
#include "blueprint.h"
#include "elaborator.h"
#include "linker.h"
#include "parser.h"
#include "subgraph.h"
#include "tokenizer.h"
#include "tui.h"
#include "waveform.h"

#define PULSE_VERSION "1.0.0"

using namespace Pulse;
using namespace Pulse::Parser;
using namespace Pulse::Engine;
using namespace Pulse::Debugger;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// Command-line options of a simulation.
struct Options
{
    std::string projectPath;            ///< The path to the project directory (first positional argument)
    bool recursive = false;             ///< Search for VHDL files in subdirectories too (-R, --recursive)
    std::string topEntity = "top";      ///< The top-level entity to simulate (--top) [lowercased]
    std::string architecture;           ///< The architecture of the top entity (--arch) [lowercased]; empty: its latest one
    simTime_t endTime = 1000;           ///< The end time of the simulation in femtoseconds (--end)
    LogicMode logic = LogicMode::Logic; ///< -Ologic: std_logic as 01XZ logic. The only mode for now, so it is always on.
};

/// Parses a VHDL file and returns its abstract syntax tree.
/// @param filename The path to the VHDL file to parse.
/// @returns AST representing the parsed file.
ASTRoot fileParsingPipeline(const std::string& filename);

/// Prints the help message to the console.
void printHelp();

/// Prints the version information to the console.
void printVersion();

/// Parses command-line arguments into the options of the simulation. Exits with an error for an unknown option.
/// @param argc The number of command-line arguments.
/// @param argv The array of command-line arguments; argv[1] is the project path.
Options parseArgs(int argc, char* argv[]);

/// Fills the sources vector with the paths of all VHDL files found in the specified project path.
/// @param projectPath The path to the project directory to search for VHDL files.
/// @param recursive Whether to recursively search for VHDL files in subdirectories.
/// @param[out] sources Output vector that will be filled with the paths of found VHDL files, in a stable order.
void getFilenamesFromProjectPath(const std::string& projectPath, bool recursive, std::vector<std::filesystem::path>& sources);

/// Prints a compiler diagnostic with its location, and the file it comes from when it is known.
void printDiagnostic(const compiler_error& error, const std::string& file = "");

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
    const std::string first = argc > 1 ? argv[1] : "";

    // $ pulse --help ...
    if (first == "-h" || first == "--help")
    {
        printHelp();
        return 0;
    }

    // $ pulse --version ...
    if (first == "-v" || first == "--version")
    {
        printVersion();
        return 0;
    }

    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <project_path> [options]\n";
        return 1;
    }

    const Options options = parseArgs(argc, argv);

    std::vector<std::filesystem::path> sources;
    try
    {
        getFilenamesFromProjectPath(options.projectPath, options.recursive, sources);
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        std::cerr << "Error: cannot read the project directory '" << options.projectPath << "': " << e.what() << '\n';
        return 1;
    }

    if (sources.empty())
    {
        std::cerr << "Error: no VHDL files (.vhd, .vhdl) found in '" << options.projectPath << "'.\n";
        return 1;
    }

    // --------------------------------------------------------------------------------------------

    try
    {
        // Every file is parsed on its own.
        std::vector<ASTRoot> files;
        for (const auto& source : sources)
        {
            try
            {
                files.push_back(fileParsingPipeline(source.string()));
            }
            catch (const compiler_error& e)
            {
                printDiagnostic(e, source.string());
                return 1;
            }
        }

        // Every file is analyzed into the work library after the files that declare the entities it needs.
        DesignLibrary work;
        for (size_t index : analysisOrder(files))
        {
            try
            {
                work.analyze(files[index]);
            }
            catch (const compiler_error& e)
            {
                printDiagnostic(e, sources[index].string());
                return 1;
            }
        }

        // Linking binds every component instance to its entity and merges the files into one design.
        Linker linker(work);
        for (auto& file : files)
            linker.addAST(std::move(file));
        const ASTRoot linkedDesign = linker.link();

        // Elaboration lowers the design, from its top entity, to blueprints of logic components.
        ElaborationOptions elaboration;
        elaboration.topEntity = options.topEntity;
        elaboration.topArchitecture = options.architecture;
        elaboration.logic = options.logic;
        const ElaboratedDesign design = elaborate(work, linkedDesign, elaboration);

        // The top-level subgraph is simulated one tick (one femtosecond) at a time.
        Subgraph graph(*design.top);
        graph.tick();
        WaveformRecorder recorder(graph.takeSnapshot());
        for (simTime_t time = 1; time <= options.endTime; ++time)
        {
            graph.tick();
            recorder.record(graph.takeSnapshot(), time);
        }

        // Once simulated, allow user to visualize the waveform of the simulation.
        showWaveform(recorder.waveform(), 0, options.endTime + 1, options.topEntity);
    }

    // --------------------------------------------------------------------------------------------

    catch (const compiler_error& e)
    {
        printDiagnostic(e);
        return 1;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
    catch (...)
    {
        std::cerr << "Error: Unknown error occurred.\n";
        return 1;
    }
    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ASTRoot fileParsingPipeline(const std::string& filename)
{
    std::ifstream inputFile(filename);
    if (!inputFile.is_open())
    {
        std::cerr << "Could not open file " << filename << ".\n";
        std::exit(1);
    }

    Tokenizer tokenizer(inputFile);
    return VHDLtoAST(tokenizer);
}

void printDiagnostic(const compiler_error& error, const std::string& file)
{
    std::cerr << "Error";
    if (!file.empty())
        std::cerr << " in " << file;
    std::cerr << " (line " << error.location().line << ", column " << error.location().column << "): " << error.what() << '\n';
}

void printHelp()
{
    std::cout << "Usage: <project_path> [options]\n";
    std::cout << "Options:\n";
    std::cout << "  -h, --help          Show this help message and exit.\n";
    std::cout << "  -v, --version       Show the program version and exit.\n\n";

    std::cout << "  -R, --recursive     Recursively search for VHDL files in subdirectories of the specified project path.\n";
    std::cout << "  --top <name>        Specify the top-level entity to simulate. (defaults to \"top\")\n";
    std::cout << "  --end <time>        Specify the end time for the simulation. (defaults to 1000fs)\n";
    std::cout << "  --arch <name>       Specify the architecture of the top-level entity. (defaults to its most recently analyzed one)\n";
    std::cout << "  -Ologic             Simulate std_logic as 01XZ logic: '0'/'L' -> 0, '1'/'H' -> 1, 'Z' -> Z, 'U'/'X'/'W'/'-' -> X.\n";
    std::cout << "                      (the only mode supported for now, so it is always on)\n";
}

void printVersion()
{
    std::cout << "Pulse Simulator. Version " << PULSE_VERSION << "\n";
}

Options parseArgs(int argc, char* argv[])
{
    Options options;
    options.projectPath = argv[1];

    const auto lowercase = [](std::string text)
    {
        for (auto& c : text)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return text;
    };

    for (int i = 2; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "-R" || arg == "--recursive")
        {
            options.recursive = true;
        }
        else if (arg == "-Ologic")
        {
            options.logic = LogicMode::Logic;
        }
        else if (arg == "--top" && i + 1 < argc)
        {
            options.topEntity = lowercase(argv[++i]);
        }
        else if (arg == "--arch" && i + 1 < argc)
        {
            options.architecture = lowercase(argv[++i]);
        }
        else if (arg == "--end" && i + 1 < argc)
        {
            // arg is XXXfs, ps, ns, us, ms, s
            // No unit means femtoseconds.
            const std::string timeStr = argv[++i];

            const size_t pos = timeStr.find_first_not_of("0123456789");
            const std::string numberPart = timeStr.substr(0, pos);
            const std::string unitPart = (pos != std::string::npos) ? timeStr.substr(pos) : "fs";

            simTime_t timeValue = 0;
            try
            {
                timeValue = std::stoull(numberPart);
            }
            catch (const std::exception&)
            {
                std::cerr << "Invalid time value: " << timeStr << "\n";
                std::exit(1);
            }

            if (unitPart == "fs")
                options.endTime = timeValue;
            else if (unitPart == "ps")
                options.endTime = timeValue * 1000;
            else if (unitPart == "ns")
                options.endTime = timeValue * 1000000;
            else if (unitPart == "us")
                options.endTime = timeValue * 1000000000;
            else if (unitPart == "ms")
                options.endTime = timeValue * 1000000000000;
            else if (unitPart == "s")
                options.endTime = timeValue * 1000000000000000;
            else
            {
                std::cerr << "Unknown time unit: " << unitPart << ". Allowed units are fs, ps, ns, us, ms, s.\n";
                std::exit(1);
            }
        }
        else
        {
            std::cerr << "Unknown option: " << arg << ". Use '--help' for more information.\n";
            std::exit(1);
        }
    }
    return options;
}

void getFilenamesFromProjectPath(const std::string& projectPath, bool recursive, std::vector<std::filesystem::path>& sources)
{
    const auto isVhdl = [](const std::filesystem::directory_entry& entry)
    {
        const auto extension = entry.path().extension();
        return entry.is_regular_file() && (extension == ".vhd" || extension == ".vhdl");
    };

    if (recursive)
    {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(projectPath))
            if (isVhdl(entry))
                sources.push_back(entry.path());
    }
    else
    {
        for (const auto& entry : std::filesystem::directory_iterator(projectPath))
            if (isVhdl(entry))
                sources.push_back(entry.path());
    }

    // Directory listings have no guaranteed order; files that do not depend on each other are analyzed in this one.
    std::sort(sources.begin(), sources.end());
}

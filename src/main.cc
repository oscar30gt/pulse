///
/// Pulse Simulator
/// Óscar Grimal Torres
///
/// Pulse is a multi-platform digital logic simulation engine for VHDL made with C++. 
/// It transforms VHDL source code into a logic components simulation model that can be simulated and debugged.
///
/// Usage: ./pulse <project_path> [options]
///

#include <fstream>
#include <cstdlib>
#include <iostream>
#include <string>
#include <filesystem>

#include "AST.h"
#include "blueprint.h"
#include "blueprintGenerator.h"
#include "linker.h"
#include "semanticAnalyzer.h"
#include "subgraph.h"
#include "tokenizer.h"
#include "waveform.h"
#include "tui.h"

#define VERSION "1.0.0"

using namespace Pulse;
using namespace Pulse::Parser;
using namespace Pulse::Engine;
using namespace Pulse::Debugger;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// Parses a VHDL file and returns its abstract syntax tree.
/// @param filename The path to the VHDL file to parse.
/// @returns AST representing the parsed file.
ASTRoot fileParsingPipeline(const std::string& filename);

/// Prints the help message to the console.
void printHelp();

/// Prints the version information to the console.
void printVersion();

/// Parses command-line arguments and sets the corresponding variables.
/// @param argc The number of command-line arguments.
/// @param argv The array of command-line arguments.
/// @param[out] projectPath The path to the project directory. (first positional argument)
/// @param[out] recursive Whether to recursively search for VHDL files in subdirectories. (set by -R or --recursive)
/// @param[out] topEntity The name of the top-level entity to simulate. Default is top. (set by --top) [lowercased]
/// @param[out] architecture The name of the architecture to use for simulation. Default is behavioral. (set by --arch) [lowercased]
/// @param[out] endTime The end time for the simulation in femtoseconds. Default is 1000fs. (set by --end)
void parseArgs(int argc, char* argv[], std::string& projectPath, bool& recursive, std::string& topEntity, std::string& architecture, simTime_t& endTime);

/// Fills the sources vector with the paths of all VHDL files found in the specified project path.
/// @param projectPath The path to the project directory to search for VHDL files.
/// @param recursive Whether to recursively search for VHDL files in subdirectories.
/// @param[out] sources Output vector that will be filled with the paths of found VHDL files.
void getFilenamesFromProjectPath(const std::string& projectPath, bool recursive, std::vector<std::filesystem::path>& sources);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
    // $ pulse --help ...
    if (argc > 1 && (argv[1] == "-h" || argv[1] == "--help"))
    {
        printHelp();
        return 0;
    }

    // $ pulse --version ...
    else if (argc > 1 && (argv[1] == "-v" || argv[1] == "--version"))
    {
        printVersion();
        return 0;
    }

    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <project_path> [options]\n";
        return 1;
    }

    std::string projectPath;
    bool recursive;
    std::string topEntity;
    std::string architecture;
    simTime_t endTime;

    parseArgs(argc, argv, projectPath, recursive, topEntity, architecture, endTime);

    std::vector<std::filesystem::path> sources;
    getFilenamesFromProjectPath(projectPath, recursive, sources);

    // --------------------------------------------------------------------------------------------

    try
    {
        // AST building for every source file.
        std::vector<ASTRoot> astRoots;
        for (const auto& source : sources)
            astRoots.push_back(fileParsingPipeline(source.string()));

        // Linking process
        Linker linker;
        auto linkedDesign = linker.link(astRoots);

        // Generating blueprints for subgraphs for the specified architecture.
        BlueprintGenerator blueprintGenerator;
        auto blueprints = blueprintGenerator.generate(linkedDesign, architecture);

        // Retreives and instantiates the top-level subgraph for simulation.
        auto bp = blueprints.find(topEntity);
        if (bp == blueprints.end())
            throw std::runtime_error("Top-level entity '" + topEntity + "' not found. Ensure it exists or provide another entity using the --top option.");

        Subgraph graph(*bp->second.get(), {}, {});
        WaveformRecorder recorder(graph.takeSnapshot());

        // Simulation
        for (simTime_t i = 0; i <= endTime; ++i)
        {
            graph.update();
            recorder.record(graph.takeSnapshot(), i);
        }

        // Once simulated, allow user to visualize the waveform of the simulation.
        showWaveform(recorder.waveform(), 0, endTime, topEntity);
    }

    // --------------------------------------------------------------------------------------------

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
    auto root = VHDLtoAST(tokenizer);
    SemanticAnalyzer semanticAnalyzer;
    semanticAnalyzer.analyze(root);
    return root;
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
    std::cout << "  --arch <name>       Specify the architecture to use when simulating the project. (defaults to \"behavioral\")\n";
}

void printVersion()
{
    std::cout << "Pulse Simulator. Version " << VERSION << "\n";
}

void parseArgs(int argc, char* argv[], std::string& projectPath, bool& recursive, std::string& topEntity, std::string& architecture, simTime_t& endTime)
{
    projectPath = argv[1];
    recursive = false;
    topEntity = "top";
    architecture = "behavioral";
    endTime = 1000; // Default end time in femtoseconds

    for (int i = 2; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-R" || arg == "--recursive")
        {
            recursive = true;
        }
        else if (arg == "--top" && i + 1 < argc)
        {
            topEntity = argv[++i];
            for (auto& c : topEntity)
                c = std::tolower(c);
        }
        else if (arg == "--arch" && i + 1 < argc)
        {
            architecture = argv[++i];
            for (auto& c : architecture)
                c = std::tolower(c);
        }
        else if (arg == "--end" && i + 1 < argc)
        {
            // arg is XXXfs, ps, ns, us, ms, s
            // No unit means femtoseconds.
            std::string timeStr = argv[++i];

            size_t pos = timeStr.find_first_not_of("0123456789");
            std::string numberPart = timeStr.substr(0, pos);
            std::string unitPart = (pos != std::string::npos) ? timeStr.substr(pos) : "fs";

            simTime_t timeValue;

            try
            {
                timeValue = std::stoull(numberPart);
            }
            catch (const std::invalid_argument&)
            {
                std::cerr << "Invalid time value: " << timeStr << "\n";
                std::exit(1);
            }

            if (unitPart == "fs")
                endTime = timeValue;
            else if (unitPart == "ps")
                endTime = timeValue * 1000;
            else if (unitPart == "ns")
                endTime = timeValue * 1000000;
            else if (unitPart == "us")
                endTime = timeValue * 1000000000;
            else if (unitPart == "ms")
                endTime = timeValue * 1000000000000;
            else if (unitPart == "s")
                endTime = timeValue * 1000000000000000;
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
}

void getFilenamesFromProjectPath(const std::string& projectPath, bool recursive, std::vector<std::filesystem::path>& sources)
{
    for (const auto& entry : std::filesystem::recursive_directory_iterator(projectPath))
    {
        if (entry.is_regular_file() &&
            (entry.path().extension() == ".vhd" || entry.path().extension() == ".vhdl"))
        {
            sources.push_back(entry.path());
        }
        else if (recursive && entry.is_directory())
        {
            getFilenamesFromProjectPath(entry.path().string(), recursive, sources);
        }
    }
}

///
/// Pulse Simulator
/// Óscar Grimal Torres
///
/// Pulse is a multi-platform digital logic simulation engine for VHDL made with C++.
/// It transforms VHDL source code into a logic components simulation model that can be simulated and debugged.
///
/// Usage: ./pulse <project_path> [options]
///

// Platform headers first: the project headers define helpers (ssize_t on MSVC) that must not leak into them.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "analyzer.h"
#include "ast.h"
#include "blueprint.h"
#include "checked_math.h"
#include "diagnostics.h"
#include "elaborator.h"
#include "linker.h"
#include "parser.h"
#include "subgraph.h"
#include "tokenizer.h"
#include "tui.h"
#include "waveform.h"

#define PULSE_VERSION "2.0.0"

using namespace Pulse;
using namespace Pulse::Parser;
using namespace Pulse::Engine;
using namespace Pulse::Debugger;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// Command-line options of a simulation.
struct Options
{
    std::string projectPath;            ///< The path to the project directory (the positional argument)
    bool recursive = false;             ///< Search for VHDL files in subdirectories too (-R, --recursive)
    std::string topEntity = "top";      ///< The top-level entity to simulate (--top) [lowercased]
    std::string architecture;           ///< The architecture of the top entity (--arch) [lowercased]; empty: its latest one
    simTime_t endTime = 1000;           ///< The end time of the simulation in femtoseconds (--end)
    LogicMode logic = LogicMode::Logic; ///< -Ologic: std_logic as 01XZ logic. The only mode for now, so it is always on.
};

/// Tokenizes and parses one VHDL file.
/// @param filename The path to the VHDL file to parse.
/// @param file The index of the file, which every location of its tree refers to.
/// @returns AST representing the parsed file.
ASTRoot fileParsingPipeline(const std::filesystem::path& filename, size_t file);

/// Prints the help message to the console.
void printHelp();

/// Prints the version information to the console.
void printVersion();

/// Parses command-line arguments into the options of the simulation. Prints the help or the version and exits when they
/// are asked for, and exits with an error for an unknown option, a missing value or a missing project path.
Options parseArgs(int argc, char* argv[]);

/// Parses the value of --end: an integer followed by fs, ps, ns, us, ms or s (fs when there is no unit).
/// Exits with an error for a malformed value or one that does not fit in the simulation time.
simTime_t parseEndTime(const std::string& text);

/// Fills the sources vector with the paths of all VHDL files found in the specified project path.
/// @param projectPath The path to the project directory to search for VHDL files.
/// @param recursive Whether to recursively search for VHDL files in subdirectories.
/// @param[out] sources Output vector that will be filled with the paths of found VHDL files, in a stable order.
void getFilenamesFromProjectPath(const std::string& projectPath, bool recursive, std::vector<std::filesystem::path>& sources);

/// Whether diagnostics written to the standard error can be colored: it is a terminal and NO_COLOR is not set.
/// On Windows it also turns on the escape sequences of the console.
bool colorsOnStandardError();

/// Prints a compiler diagnostic in the GCC style, with the file, the line and a caret when its location is known.
/// @param sources The source files of the design; the location of the error refers to one of them by index.
void printDiagnostic(const compiler_error& error, const std::vector<std::filesystem::path>& sources);

/// Prints `pulse: error: <message>`, the way command-line errors are reported, and exits with status 1.
[[noreturn]] void failWith(const std::string& message);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
    const Options options = parseArgs(argc, argv);

    std::vector<std::filesystem::path> sources;
    try
    {
        getFilenamesFromProjectPath(options.projectPath, options.recursive, sources);
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        failWith("cannot read the project directory '" + options.projectPath + "': " + e.code().message());
    }

    if (sources.empty())
        failWith("no VHDL files (.vhd, .vhdl) found in '" + options.projectPath + "'");

    // --------------------------------------------------------------------------------------------

    try
    {
        // Every file is parsed on its own; its index in `sources` travels with every location of its tree.
        std::vector<ASTRoot> files;
        for (size_t index = 0; index < sources.size(); ++index)
            files.push_back(fileParsingPipeline(sources[index], index));

        // Every file is analyzed into the work library after the files that declare the entities it needs.
        DesignLibrary work;
        for (size_t index : analysisOrder(files))
            work.analyze(files[index]);

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
        printDiagnostic(e, sources);
        return 1;
    }
    catch (const std::exception& e)
    {
        failWith(e.what());
    }
    catch (...)
    {
        failWith("unknown error");
    }
    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ASTRoot fileParsingPipeline(const std::filesystem::path& filename, size_t file)
{
    std::ifstream inputFile(filename);
    if (!inputFile.is_open())
        failWith("cannot open the file '" + filename.string() + "'");

    Tokenizer tokenizer(inputFile, file);
    return VHDLtoAST(tokenizer);
}

bool colorsOnStandardError()
{
    // NO_COLOR (https://no-color.org): any non-empty value turns colors off.
    #ifdef _MSC_VER
    char* noColor = nullptr;
    size_t length = 0;
    const bool colorsOff = _dupenv_s(&noColor, &length, "NO_COLOR") == 0 && noColor && *noColor;
    std::free(noColor);
    #else
    const char* noColor = std::getenv("NO_COLOR");
    const bool colorsOff = noColor && *noColor;
    #endif
    if (colorsOff)
        return false;

    #ifdef _WIN32
    if (!_isatty(_fileno(stderr)))
        return false;
    const HANDLE error = GetStdHandle(STD_ERROR_HANDLE);
    DWORD mode{};
    return GetConsoleMode(error, &mode) && SetConsoleMode(error, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    #else
    return isatty(STDERR_FILENO);
    #endif
}

void printDiagnostic(const compiler_error& error, const std::vector<std::filesystem::path>& sources)
{
    const SourceLocation& at = error.location();

    std::string path;
    std::optional<std::string> line;
    if (at.known() && at.file < sources.size())
    {
        path = std::filesystem::path(sources[at.file]).make_preferred().string();
        std::ifstream source(sources[at.file], std::ios::binary);
        line = sourceLineOf(source, at.line);
    }

    std::cerr << formatDiagnostic(error, path, line, colorsOnStandardError());
}

void failWith(const std::string& message)
{
    const bool color = colorsOnStandardError();
    std::cerr << (color ? "\033[1mpulse:\033[0m \033[1;31merror:\033[0m " : "pulse: error: ") << message << '\n';
    std::exit(1);
}

void printHelp()
{
    std::cout << "Usage: pulse <project_path> [options]\n";
    std::cout << "Options:\n";
    std::cout << "  -h, --help          Show this help message and exit.\n";
    std::cout << "  -v, --version       Show the program version and exit.\n\n";

    std::cout << "  -R, --recursive     Recursively search for VHDL files in subdirectories of the specified project path.\n";
    std::cout << "  --top <name>        Specify the top-level entity to simulate. (defaults to \"top\")\n";
    std::cout << "  --end <time>        Specify the end time for the simulation: an integer followed by fs, ps, ns, us, ms or s.\n";
    std::cout << "                      (defaults to 1000fs; a number without a unit is in fs)\n";
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
    // --help and --version win wherever they are.
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "-h" || arg == "--help")
        {
            printHelp();
            std::exit(0);
        }
        if (arg == "-v" || arg == "--version")
        {
            printVersion();
            std::exit(0);
        }
    }

    const auto lowercase = [](std::string text)
    {
        for (auto& c : text)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return text;
    };

    Options options;
    bool hasProjectPath = false;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];

        // The value of an option that takes one.
        const auto value = [&]() -> std::string
        {
            if (i + 1 >= argc)
                failWith("option '" + arg + "' needs a value; use '--help' for more information");
            return argv[++i];
        };

        if (arg == "-R" || arg == "--recursive")
            options.recursive = true;
        else if (arg == "-Ologic")
            options.logic = LogicMode::Logic;
        else if (arg == "--top")
            options.topEntity = lowercase(value());
        else if (arg == "--arch")
            options.architecture = lowercase(value());
        else if (arg == "--end")
            options.endTime = parseEndTime(value());
        else if (!arg.empty() && arg.front() == '-')
            failWith("unknown option '" + arg + "'; use '--help' for more information");
        else if (hasProjectPath)
            failWith("unexpected argument '" + arg + "': only one project path can be given");
        else
        {
            options.projectPath = arg;
            hasProjectPath = true;
        }
    }

    if (!hasProjectPath)
    {
        std::cerr << "Usage: pulse <project_path> [options]\n";
        failWith("no project path given; use '--help' for more information");
    }
    return options;
}

simTime_t parseEndTime(const std::string& text)
{
    // An integer followed by a unit; no unit means femtoseconds.
    const size_t pos = text.find_first_not_of("0123456789");
    const std::string numberPart = text.substr(0, pos);
    const std::string unitPart = pos != std::string::npos ? text.substr(pos) : "fs";

    if (numberPart.empty() || unitPart.find_first_not_of("abcdefghijklmnopqrstuvwxyz") != std::string::npos)
        failWith("invalid end time '" + text + "': expected an integer followed by fs, ps, ns, us, ms or s");

    static const std::pair<const char*, int64_t> units[] = {
        { "fs", 1 }, { "ps", 1'000 }, { "ns", 1'000'000 }, { "us", 1'000'000'000 }, { "ms", 1'000'000'000'000 },
        { "s", 1'000'000'000'000'000 },
    };

    const auto unit = std::find_if(std::begin(units), std::end(units), [&](const auto& u) { return unitPart == u.first; });
    if (unit == std::end(units))
        failWith("unknown time unit '" + unitPart + "' in '" + text + "'; allowed units are fs, ps, ns, us, ms and s");

    // The number is at most int64 before it is scaled to femtoseconds, and so is the result.
    std::optional<int64_t> femtoseconds;
    if (numberPart.size() <= 18 || (numberPart.size() == 19 && numberPart <= "9223372036854775807"))
        femtoseconds = checkedMul(std::stoll(numberPart), unit->second);
    if (!femtoseconds)
        failWith("end time '" + text + "' is too large (the simulation time is limited to about 2.5 hours)");

    return static_cast<simTime_t>(*femtoseconds);
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

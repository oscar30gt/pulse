#include <fstream>
#include <cstdlib>
#include <iostream>
#include <string>

#include "AST.h"
#include "blueprint.h"
#include "blueprintGenerator.h"
#include "linker.h"
#include "semanticAnalyzer.h"
#include "subgraph.h"
#include "tokenizer.h"
#include "waveform.h"

using namespace Pulse::Parser::VHDL;
using namespace Pulse::Engine;

Pulse::Parser::VHDL::ASTRoot pipeline(const std::string& filename)
{
    std::ifstream inputFile(filename);
    if (!inputFile.is_open())
    {
        std::cerr << "Error: Could not open file " << filename << ".\n";
        std::exit(1);
    }

    Tokenizer tokenizer(inputFile);
    auto root = VHDLtoAST(tokenizer);
    SemanticAnalyzer semanticAnalyzer;
    semanticAnalyzer.analyze(root);
    return root;
}

int main(int argc, char* argv[])
{
    try
    {
        auto clock = pipeline("test-project/clock.vhd");
        auto counter = pipeline("test-project/counter.vhd");
        auto testbench = pipeline("test-project/counter-circuit.vhd");

        Linker linker;
        auto linkedDesign = linker.link({ &clock, &counter, &testbench });
        BlueprintGenerator blueprintGenerator;
        auto blueprints = blueprintGenerator.generate(linkedDesign);

        auto bp = blueprints.find("countercircuit")->second.get();
        Subgraph graph(*bp, {}, {});
        Pulse::Waveform::WaveformRecorder recorder(graph.takeSnapshot());

        for (int i = 0; i < 150; ++i)
        {
            graph.update();
            recorder.record(graph.takeSnapshot(), i);
        }

        Pulse::Waveform::showWaveform(recorder.waveform(), 0, 150);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << '\n';
    }
    catch (...)
    {
        std::cerr << "Error: Unknown error occurred.\n";
    }
    return 0;
}
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <sstream>
#include "tokenizer.h"
#include "AST.h"
#include "semanticAnalyzer.h"
#include "linker.h"
#include "blueprintGenerator.h"
#include "blueprint.h"

#include "wire.h"
#include "subgraph.h"
#include "signalSource.h"
#include "logicVector.h"

using namespace Pulse::Parser::VHDL;
using namespace Pulse::Engine;
using Pulse::LogicVector;

Pulse::Parser::VHDL::ASTRoot pipeline(std::string filename)
{
    std::ifstream inputFile(filename);
    if (!inputFile.is_open())
    {
        std::cerr << "Error: Could not open file 'test.vhdl'." << std::endl;
        exit(1);
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
        auto clock = pipeline("clock.vhdl");
        // clock.print();

        Linker linker;
        auto linkedDesign = linker.link({ &clock });
        // Linker::printLinkedDesign(linkedDesign);

        BlueprintGenerator blueprintGenerator;
        auto blueprints = blueprintGenerator.generate(linkedDesign);

        for (const auto& [entityName, bp] : blueprints)
        {
            std::cout << "Blueprint for entity: " << entityName << std::endl;
            bp->print();
        }

        auto bp = blueprints.find("clock")->second.get();

        Wire outWire(1);
        Subgraph graph(*bp, {}, { {"clk_out", &outWire} });
        for (int i = 0; i < 100000; ++i)
        {
            graph.update();
            std::cout << "Time: " << std::setw(5) << std::setfill('0') << std::right << i
                << "fs, clk_out: " << outWire.peek().str()
                << std::endl;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "Error: Unknown error occurred." << std::endl;
    }

    return 0;
}

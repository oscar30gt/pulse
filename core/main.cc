#include <iostream>
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
        clock.print();
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

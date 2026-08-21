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
    ASTBuilder astBuilder(tokenizer);

    SemanticAnalyzer semanticAnalyzer;
    auto root = std::move(astBuilder.getRoot());
    semanticAnalyzer.analyze(root);
    return root;
}

int main(int argc, char* argv[])
{
    try
    {
        auto mux = pipeline("mux.vhdl");
        auto alu = pipeline("alu.vhdl");

        Linker linker;
        auto linkedGraph = linker.link({ &mux, &alu });
        Linker::printLinkedDesign(linkedGraph);

        BlueprintGenerator blueprintGenerator;
        auto blueprints = blueprintGenerator.generate(linkedGraph);
        for (const auto& [archName, blueprint] : blueprints)
        {
            std::cout << "Blueprint for architecture: " << archName << std::endl;
            blueprint->print(std::cout);
        }

        auto aluBp = blueprints.find("alu")->second.get();
        
        Wire a(32), b(32), sel(2), out(32);
        SignalSource sourceA(32), sourceB(32), sourceSel(2);
        sourceA.addTarget(&a);
        sourceB.addTarget(&b);
        sourceSel.addTarget(&sel);

        Subgraph aluSubgraph(*aluBp, {{"a", &a}, {"b", &b}, {"sel", &sel}}, {{"res", &out}});

        sourceA.drive(LogicVector::FromInt(0b0010));
        sourceB.drive(LogicVector::FromInt(0b1010));

        sourceSel.drive(LogicVector::FromInt(0));
        std::cout << "Output: " << out.peek().str() << std::endl;
        sourceSel.drive(LogicVector::FromInt(1));
        std::cout << "Output: " << out.peek().str() << std::endl;
        sourceSel.drive(LogicVector::FromInt(2));
        std::cout << "Output: " << out.peek().str() << std::endl;
        sourceSel.drive(LogicVector::FromInt(3));
        std::cout << "Output: " << out.peek().str() << std::endl;
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

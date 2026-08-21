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

#include "x.cc"

using namespace Pulse::Parser::VHDL;
using namespace Pulse::Engine;
using Pulse::LogicVector;

std::unique_ptr<Pulse::Parser::Blueprint> pipeline(std::string filename)
{
    std::ifstream inputFile(filename);
    if (!inputFile.is_open())
    {
        std::cerr << "Error: Could not open file 'test.vhdl'." << std::endl;
        exit(1);
    }

    Tokenizer tokenizer(inputFile);
    ASTBuilder astBuilder(tokenizer);
    // astBuilder.printTree();

    SemanticAnalyzer semanticAnalyzer;
    semanticAnalyzer.analyze(astBuilder.getRoot());

    Linker linker;
    std::vector<const RootNode*> astRoots;
    astRoots.push_back(&astBuilder.getRoot());
    LinkedDesign linkedDesign = linker.link(astRoots);
    // Linker::printLinkedDesign(linkedDesign);

    Pulse::Parser::VHDL::BlueprintGenerator generator;
    auto blueprints = generator.generate(linkedDesign);

    // Access your target architecture (e.g., "behavioral")
    auto bp = std::move(blueprints["behavioral"]);
    Pulse::Parser::BlueprintPrinter::print(*bp);
    return bp;
}


int main(int argc, char* argv[])
{
    std::cout << "\n\n======== FLIP FLOP ============================================================================" << std::endl;
    try
    {
        auto bp = pipeline("flipflop.vhdl");

        Wire set(1), reset(1), q(1), qNot(1);
        SignalSource srcSet(1), srcReset(1);
        srcSet.addTarget(&set);
        srcReset.addTarget(&reset);

        Subgraph flipFlop(*bp.get(), { {"set", &set}, {"reset", &reset} }, { {"q", &q}, {"qnot", &qNot} });

        srcSet.drive(LogicVector::FromBool(0));
        srcReset.drive(LogicVector::FromBool(0));

        srcSet.drive(LogicVector::FromBool(1)); // Set
        std::cout << std::boolalpha << "Q: " << (bool)q.peek() << ", QNot: " << (bool)qNot.peek() << " Expected: true, false" << std::endl;
        srcSet.drive(LogicVector::FromBool(0));

        srcReset.drive(LogicVector::FromBool(1)); // Reset
        std::cout << std::boolalpha << "Q: " << (bool)q.peek() << ", QNot: " << (bool)qNot.peek() << " Expected: false, true" << std::endl;
        srcReset.drive(LogicVector::FromBool(0));

        srcSet.drive(LogicVector::FromBool(1)); // Set again
        std::cout << std::boolalpha << "Q: " << (bool)q.peek() << ", QNot: " << (bool)qNot.peek() << " Expected: true, false" << std::endl;
        srcSet.drive(LogicVector::FromBool(0));

    }
    catch (const std::exception& e)
    {
        std::cerr << "Parsing error: " << e.what() << std::endl;
    }

    std::cout << "\n\n======== MUX ==================================================================================" << std::endl;
    try
    {
        auto bp = pipeline("mux.vhdl");

        Wire in0(32), in1(32), in2(32), in3(32), sel(2), out(32);
        SignalSource src0(32), src1(32), src2(32), src3(32), srcSel(2);
        src0.addTarget(&in0);
        src1.addTarget(&in1);
        src2.addTarget(&in2);
        src3.addTarget(&in3);
        srcSel.addTarget(&sel);

        Subgraph flipFlop(*bp.get(), { {"in0", &in0}, {"in1", &in1}, {"in2", &in2}, {"in3", &in3}, {"sel", &sel} }, { {"_out", &out} });

        src0.drive(LogicVector::FromInt(0x100));
        src1.drive(LogicVector::FromInt(0x200));
        src2.drive(LogicVector::FromInt(0x300));
        src3.drive(LogicVector::FromInt(0x400));
        srcSel.drive(LogicVector::FromInt(0));

        std::cout << "Selected Output: " << std::hex << out.peek().str() << " Expected: 0xaaaa" << std::endl;
        srcSel.drive(LogicVector::FromInt(1));
        std::cout << "Selected Output: " << std::hex << out.peek().str() << " Expected: 0xbbbb" << std::endl;
        srcSel.drive(LogicVector::FromInt(2));
        std::cout << "Selected Output: " << std::hex << out.peek().str() << " Expected: 0xcccc" << std::endl;
        srcSel.drive(LogicVector::FromInt(3));
        std::cout << "Selected Output: " << std::hex << out.peek().str() << " Expected: 0xdddd" << std::endl;
        srcSel.drive(LogicVector::FromInt(2));
        std::cout << "Selected Output: " << std::hex << out.peek().str() << " Expected: 0xcccc" << std::endl;
        srcSel.drive(LogicVector::FromInt(0));
        std::cout << "Selected Output: " << std::hex << out.peek().str() << " Expected: 0xaaaa" << std::endl;

       
    }
    catch (const std::exception& e)
    {
        std::cerr << "Parsing error: " << e.what() << std::endl;
    }

    // std::cout << "\n\n======== ALU ==================================================================================" << std::endl;
    // try
    // {
    //     pipeline("alu.vhdl");
    // }
    // catch (const std::exception& e)
    // {
    //     std::cerr << "Parsing error: " << e.what() << std::endl;
    // }

    return 0;
}

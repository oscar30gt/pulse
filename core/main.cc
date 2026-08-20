#include <iostream>
#include <fstream>
#include <string>
#include "tokenizer.h"
#include "ASTBuilder.h"
#include "utils.cc"

using namespace Pulse::Parser::VHDL;

int main(int argc, char* argv[])
{
    std::ifstream inputFile("test.vhdl");
    if (!inputFile.is_open())
    {
        std::cerr << "Error: Could not open file 'test.vhdl'." << std::endl;
        return 1;
    }

    Tokenizer tokenizer(inputFile);
    try {
        ASTBuilder astBuilder(tokenizer);
        ASTPrinter::printTree(astBuilder.getRoot());
    } catch (const std::exception& e) {
        std::cerr << "Parsing error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "An unknown error occurred during parsing." << std::endl;
        return 1;
    }
}

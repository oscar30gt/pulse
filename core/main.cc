#include <iostream>

// ./pulse <input_file> [--cli]
int main(int argc, char* argv[]) {
    
    
    if (argc < 2) {
        std::cerr << "Usage: ./pulse <input_file> [--cli]" << std::endl;
        return 1;
    }
    
    std::string input_file = argv[1];
    bool cli_mode = false;
    
    if (argc > 2 && std::string(argv[2]) == "--cli") {
        cli_mode = true;
    }



}
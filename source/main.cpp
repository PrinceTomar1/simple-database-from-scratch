// simple-database-from-scratch
// entry point for the repl.

#include <iostream>
#include <string>

#include "parser.h"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    std::cout << "simple db engine - type EXIT to quit" << std::endl;

    std::string line;
    while (true) {
        std::cout << "db> ";
        if (!std::getline(std::cin, line)) {
            break;
        }

        Statement stmt = parseStatement(line);

        if (stmt.type == StatementType::EMPTY) {
            continue;
        }
        if (stmt.type == StatementType::EXIT) {
            break;
        }
        if (stmt.type == StatementType::PARSE_ERROR) {
            std::cout << "error: " << stmt.errorMessage << std::endl;
            continue;
        }

        std::cout << "recognized statement, execution not wired up yet" << std::endl;
    }

    return 0;
}

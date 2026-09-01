// simple-database-from-scratch
// entry point for the repl. right now this is just a skeleton that
// reads lines and prints them back - real parsing comes next.

#include <iostream>
#include <string>

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
        if (line == "EXIT" || line == "exit") {
            break;
        }
        if (line.empty()) {
            continue;
        }
        std::cout << "not implemented yet: " << line << std::endl;
    }

    return 0;
}

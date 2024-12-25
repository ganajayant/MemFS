#include "CommandInterpreter.hpp"
#include <iostream>
#include <signal.h>
#include <string>

using namespace std;

string getHeader() {
    return "memfs> ";
}

void handle_sigint(int) {
    cout << "\nuse exit command to exit or use Ctrl + d\n"
         << getHeader();
    cout.flush();
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    signal(SIGINT, handle_sigint);
    cout << "\033[2J\033[1;1H";
    CommandInterpreter parser(4);
    for (string line; cout << getHeader() && getline(cin, line);) {
        if (!line.empty()) {
            parser.process(line);
        }
    }
    return 0;
}
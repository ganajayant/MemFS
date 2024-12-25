#ifndef CI_HPP
#define CI_HPP

#include "MemFS.hpp"
#include <cstddef>
#include <ctime>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using namespace std::chrono;

class ValidationResult {
public:
    bool success;
    string errmsg;
    vector<string> filenames;
    vector<string> contents;
    bool long_format = false;
    int file_count = 0;
};

class CommandInterpreter {
private:
    MemFS fs;

    static vector<string> split(const string &str) {
        vector<string> tokens;
        string token;
        bool inQuotes = false;
        for (size_t i = 0; i < str.length(); ++i) {
            char c = str[i];
            if (c == '"') {
                inQuotes = !inQuotes;
            } else if (isspace(c) && !inQuotes) {
                if (!token.empty()) {
                    tokens.push_back(token);
                    token.clear();
                }
            } else {
                token += c;
            }
        }
        if (!token.empty()) {
            tokens.push_back(token);
        }
        return tokens;
    }

    void help_menu() {
        cout << "Available commands:" << endl
             << "  create <filename>                             - Create a new file with the specified filename" << endl
             << "  create -n <count> <filenames...>              - Create multiple files; expects <count> filenames" << endl
             << "  write <filename> \"<content>\"                  - Write content to a file" << endl
             << "  write -n <count> <filename> \"<content>\" ...   - Write to multiple files; expects <count> filename/content pairs" << endl
             << "  read <filename>                               - Read and display the content of a file" << endl
             << "  delete <filename>                             - Delete a specific file" << endl
             << "  delete -n <count> <filenames...>              - Delete multiple files; expects <count> filenames" << endl
             << "  ls                                            - List directory contents" << endl
             << "  ls -l                                         - List directory contents in long format" << endl
             << "  help                                          - Show this help menu" << endl
             << "  exit                                          - Exit the program" << endl
             << "  clear                                         - Clear the screen" << endl;
    }

    static bool validateCreate(const string &input, ValidationResult &result, const vector<string> &tokens) {
        static const regex createSimple(R"(^create\s+[a-zA-Z0-9_]+\.[a-zA-Z0-9]+$)");
        static const regex createMultiple(R"(^create\s+-n\s+(\d+)\s+(.+)$)");
        static const regex validFilename(R"([a-zA-Z0-9_]+\.[a-zA-Z0-9]+)");

        if (regex_match(input, createSimple)) {
            result.success = true;
            result.file_count = 1;
            result.filenames.push_back(input.substr(7));
            return true;
        }

        smatch matches;
        if (regex_match(input, matches, createMultiple)) {
            int expectedFiles = stoi(matches[1]);
            istringstream iss(matches[2].str());

            for (size_t i = 3; i < tokens.size(); ++i) {
                if (!regex_match(tokens[i], validFilename)) {
                    result.success = false;
                    result.errmsg = "Invalid filename: " + tokens[i];
                    return false;
                }
                result.filenames.push_back(tokens[i]);
            }

            result.success = (result.filenames.size() == static_cast<size_t>(expectedFiles));
            if (!result.success) {
                result.errmsg = "Expected " + to_string(expectedFiles) +
                                " files, but got " + to_string(result.filenames.size());
            }
            result.file_count = result.filenames.size();
            return result.success;
        }

        result.success = false;
        result.errmsg = "Invalid command format. Use 'help' command for usage";
        return false;
    }

    static bool validateWrite(const string &input, ValidationResult &result, const vector<string> &tokens) {
        static const regex writeSimple(R"(^write\s+([a-zA-Z0-9_]+\.[a-zA-Z0-9_]+)\s+\"([^"]*)\"$)");
        static const regex writeMultiple(R"(^write\s+-n\s+(\d+)\s+(.+)$)");
        static const regex validFilename(R"([a-zA-Z0-9_]+\.[a-zA-Z0-9]+)");
        smatch matches;

        if (regex_match(input, matches, writeSimple)) {
            result.success = true;
            result.file_count = 1;
            result.filenames.push_back(matches[1]);
            result.contents.push_back(matches[2]);
            return true;
        }

        if (regex_match(input, matches, writeMultiple)) {
            int expectedFiles = stoi(matches[1]);
            istringstream iss(matches[2].str());
            string token;
            bool expectingFilename = true;

            for (size_t i = 3; i < tokens.size(); ++i) {
                if (expectingFilename) {
                    if (!regex_match(tokens[i], validFilename)) {
                        result.success = false;
                        result.errmsg = "Invalid filename: " + tokens[i];
                        return false;
                    }
                    result.filenames.push_back(tokens[i]);
                    expectingFilename = false;
                } else {
                    result.contents.push_back(tokens[i]);
                    expectingFilename = true;
                }
            }

            result.success = (result.filenames.size() == static_cast<size_t>(expectedFiles) && result.contents.size() == static_cast<size_t>(expectedFiles));
            if (!result.success) {
                result.errmsg = "Expected " + to_string(expectedFiles) +
                                " files and contents, but got " + to_string(result.filenames.size()) +
                                " files and " + to_string(result.contents.size()) + " contents.";
            }
            result.file_count = result.filenames.size();
            return result.success;
        }

        result.success = false;
        result.errmsg = "Invalid command format. Use 'help' command for usage";
        return false;
    }

    static bool validateLs(const string &input, ValidationResult &result) {
        if (input != "ls" && input != "ls -l") {
            result.success = false;
            result.errmsg = "Invalid command format. Use 'help' command for usage";
            return false;
        }
        result.success = true;
        result.long_format = (input == "ls -l");
        return true;
    }

    static bool validateRead(const string &input, ValidationResult &result) {
        static const regex read(R"(^read\s+([a-zA-Z0-9_]+\.[a-zA-Z0-9_]+)$)");
        smatch matches;

        if (regex_match(input, matches, read)) {
            result.success = true;
            result.file_count = 1;
            result.filenames.push_back(matches[1]);
            return true;
        }

        result.success = false;
        result.errmsg = "Invalid command format. Use 'help' command for usage";
        return false;
    }
    static bool validateDelete(const string &input, ValidationResult &result, const vector<string> &tokens) {
        static const regex deleteFile(R"(^delete\s+([a-zA-Z0-9_]+\.[a-zA-Z0-9_]+)$)");
        static const regex deleteMultiple(R"(^delete\s+-n\s+(\d+)\s+(.+)$)");
        static const regex validFilename(R"([a-zA-Z0-9_]+\.[a-zA-Z0-9]+)");
        smatch matches;

        if (regex_match(input, matches, deleteFile)) {
            result.success = true;
            result.file_count = 1;
            result.filenames.push_back(matches[1]);
            return true;
        }

        if (regex_match(input, matches, deleteMultiple)) {
            int expectedFiles = stoi(matches[1]);
            istringstream iss(matches[2].str());

            for (size_t i = 3; i < tokens.size(); ++i) {
                if (!regex_match(tokens[i], validFilename)) {
                    result.success = false;
                    result.errmsg = "Invalid filename: " + tokens[i];
                    return false;
                }
                result.filenames.push_back(tokens[i]);
            }

            result.success = (result.filenames.size() == static_cast<size_t>(expectedFiles));
            if (!result.success) {
                result.errmsg = "Expected " + to_string(expectedFiles) +
                                " files, but got " + to_string(result.filenames.size());
            }
            result.file_count = result.filenames.size();
            return result.success;
        }

        result.success = false;
        result.errmsg = "Invalid command format. Use 'help' command for usage";
        return false;
    }

public:
    CommandInterpreter(size_t thread_count) : fs(thread_count) {}
    void process(const string &line) {
        try {
            vector<string> tokens = split(line);
            if (tokens.empty()) {
                return;
            }
            const string &command = tokens[0];
            if (command == "create") {
                ValidationResult result;
                if (!validateCreate(line, result, tokens)) {
                    throw runtime_error(result.errmsg);
                }
                int number_of_files = result.file_count;
                vector<string> filenames = result.filenames;
                fs.create_files(number_of_files, filenames);
            } else if (command == "write") {
                ValidationResult result;
                if (!validateWrite(line, result, tokens)) {
                    throw runtime_error(result.errmsg);
                }
                int number_of_files = result.file_count;
                vector<string> filenames = result.filenames;
                vector<string> contents = result.contents;
                fs.write_files(number_of_files, filenames, contents);
            } else if (command == "read") {
                ValidationResult result;
                if (!validateRead(line, result)) {
                    throw runtime_error(result.errmsg);
                }
                string filename = result.filenames[0];
                fs.read_file(filename);
            } else if (command == "delete") {
                ValidationResult result;
                if (!validateDelete(line, result, tokens)) {
                    throw runtime_error(result.errmsg);
                }
                int number_of_files = result.file_count;
                vector<string> filenames = result.filenames;
                fs.delete_files(number_of_files, filenames);
            } else if (command == "ls") {
                ValidationResult result;
                if (!validateLs(line, result)) {
                    throw runtime_error(result.errmsg);
                }
                bool long_format = result.long_format;
                fs.ls(long_format);
            } else if (command == "help") {
                help_menu();
            } else if (command == "exit") {
                cout << "exiting memFS" << endl;
                exit(0);
            } else if (command == "clear") {
                cout << "\033[2J\033[1;1H";
            } else {
                throw runtime_error("Unknown command: " + command);
            }
        } catch (const exception &e) {
            cerr << "Error: " << e.what() << endl;
        }
    }
};

#endif
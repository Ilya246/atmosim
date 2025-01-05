#include "args.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdlib>

std::shared_ptr<base_argument> make_argument(std::string long_name, std::string alias, std::string description, bool& value) {
    return std::make_shared<flag_argument>(long_name, alias, description, value);
}

void parse_arguments(const std::vector<std::shared_ptr<base_argument>>& args, int argc, char* argv[]) {
    std::vector<std::string> argv_str(argv, argv + argc);
    std::map<std::string, std::shared_ptr<base_argument>> arg_map;
    for (const std::shared_ptr<base_argument>& a : args) {
        arg_map[a->long_name] = a;
        if (a->alias != "") {
            arg_map[a->alias] = a;
        }
    }
    bool errored = false, do_help = false;
    std::string errors;
    for (size_t i = 1; i < argv_str.size(); ++i) {
        std::string arg = argv_str[i];
        if (arg.size() < 2 || arg[0] != '-') {
            errors += "Bad argument: " + arg + '\n';
            errored = true;
            continue;
        }
        arg = arg.substr(1);
        if (arg[0] == '-') arg = arg.substr(1);

        do_help |= arg == "--help" || arg == "-h";

        size_t equals_pos = arg.find('=');
        if (equals_pos != std::string::npos) {
            arg = arg.substr(0, equals_pos);
        }
        if (arg_map.count(arg) != 0) {
            arg_map[arg]->parse(argv_str, i);
        } else {
            errors += "Unknown argument: " + arg + '\n';
            errored = true;
        }
    }
    if (do_help) {
        for (const std::shared_ptr<base_argument>& a : args) {
            std::cout << a->help() << std::endl;
        }
    }
    if (errored) {
        std::cerr << "Couldn't parse arguments:\n" << errors;
        exit(1);
    }
    if (do_help) exit(0);
}

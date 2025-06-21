#pragma once

#include "read.hpp"

#include <cxxabi.h>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace argp {

struct base_argument {
    std::string long_name;
    std::string alias;
    std::string description;

    base_argument(std::string long_name, std::string alias, std::string description)
        : long_name(long_name), alias(alias), description(description) {}
    virtual ~base_argument() = default;

    virtual void parse(const std::vector<std::string>& argv, size_t& index) = 0;
    virtual std::string help() const = 0;
};

struct parse_error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

template<typename T>
struct value_argument : base_argument {
    T& value;
    T default_value;
    bool has_default = false;

    value_argument(std::string long_name, std::string alias, std::string description, T& a_value)
        : base_argument(long_name, alias, description), value(a_value) {}

    value_argument(std::string long_name, std::string alias, std::string description, T& a_value, const T& a_default_value)
        : base_argument(long_name, alias, description), value(a_value), default_value(a_default_value), has_default(true) {}

    // parse argument: may seek forward, index will be set to latest element parsed
    void parse(const std::vector<std::string>& argv, size_t& index) override {
        std::string_view arg = argv[index];

        std::string_view val_str;
        size_t equals_pos = arg.find('=');
        // --arg=val
        if (equals_pos != std::string::npos) {
            val_str = arg.substr(equals_pos + 1);
        // --arg val
        } else {
            ++index;
            bool has_value = index < argv.size() && (val_str = argv[index]).size() > 0 && val_str[0] != '-';
            if (!has_value) --index;
            if (!has_value && has_default) {
                // after us is end or another arg, check if we have a default
                if (has_default) {
                    value = default_value;
                    return;
                }
                // no default
                else throw parse_error("No value for argument " + std::string(arg));
            }
        }

        try {
            value = parse_value<T>(val_str);
        } catch (const read_error& e) {
            throw parse_error("Failed to parse value for " + std::string(arg) + ": " + e.what());
        }
    }

    std::string help() const override {
        // --arg=T (alias -a): does something
        return "--" + long_name + "=" + type_sig<T> + (alias == "" ? " " : " (alias -" + alias + "): ") + description;
    }
};

template<typename T>
inline std::shared_ptr<value_argument<T>> make_argument(std::string long_name, std::string alias, std::string description, T& value) {
    return std::make_shared<value_argument<T>>(long_name, alias, description, value);
}
template<typename T>
inline std::shared_ptr<value_argument<T>> make_argument(std::string long_name, std::string alias, std::string description, T& value, T default_value) {
    return std::make_shared<value_argument<T>>(long_name, alias, description, value, default_value);
}
// make bools have a default of true if unspecified
template<>
inline std::shared_ptr<value_argument<bool>> make_argument<bool>(std::string long_name, std::string alias, std::string description, bool& value) {
    return std::make_shared<value_argument<bool>>(long_name, alias, description, value, true);
}

inline void parse_arguments(const std::vector<std::shared_ptr<base_argument>>& args, int argc, char* argv[], std::string pre_help = "", std::string post_help = "") {
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

        if (arg == "help" || arg == "h") {
            do_help = true;
            continue;
        }

        size_t equals_pos = arg.find('=');
        if (equals_pos != std::string::npos) {
            arg = arg.substr(0, equals_pos);
        }
        if (arg_map.count(arg) != 0) {
            try {
                arg_map[arg]->parse(argv_str, i);
            } catch (const parse_error& e) {
                errors += std::string(e.what()) + '\n';
                errored = true;
            }
        } else {
            errors += "Unknown argument: " + arg + '\n';
            errored = true;
            do_help = true;
        }
    }
    if (errored) {
        std::cerr << "Couldn't parse arguments:\n" << errors;
    }
    if (do_help) {
        if (!pre_help.empty()) {
            std::cout << pre_help << '\n';
        }
        std::cout << "All flags: " << '\n';
        for (const std::shared_ptr<base_argument>& a : args) {
            std::cout << "  " << a->help() << '\n';
        }
        if (!post_help.empty()) {
            std::cout << post_help << '\n';
        }
        std::flush(std::cout);
    }
    if (errored || do_help) exit(errored);
}

}

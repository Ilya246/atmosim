#pragma once

#include <memory>
#include <sstream>
#include <string>
#include <vector>

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

struct flag_argument : base_argument {
    bool& value;

    flag_argument(std::string long_name, std::string alias, std::string description, bool& value)
        : base_argument(long_name, alias, description), value(value) {}

    void parse(const std::vector<std::string>& argv, size_t& index) override {
        std::string arg = argv[index];

        value = true;
        size_t equals_pos = arg.find('=');
        if (equals_pos != std::string::npos) {
            std::istringstream read_s(arg.substr(equals_pos + 1));
            int val_to;
            read_s >> val_to;
            if (!read_s || !read_s.eof()) throw std::runtime_error("Failed to parse value for " + long_name);
            value = (bool)val_to;
        }
    }
    std::string help() const override {
        return "--" + long_name + (alias == "" ? "" : "(-" + alias + ")") + ": " + description;
    }
};

template<typename T>
struct value_argument : base_argument {
    T& value;

    value_argument(std::string long_name, std::string alias, std::string description, T& value)
        : base_argument(long_name, alias, description), value(value) {}

    void parse(const std::vector<std::string>& argv, size_t& index) override {
        std::string arg = argv[index];

        std::string val;
        size_t equals_pos = arg.find('=');
        if (equals_pos != std::string::npos) {
            val = arg.substr(equals_pos + 1);
        } else {
            ++index;
            val = argv[index];
        }
        std::istringstream read_s(val);
        read_s >> value;
        if (!read_s || !read_s.eof()) throw std::runtime_error("Failed to parse value for " + long_name);
    }
    std::string help() const override {
        return "--" + long_name + (alias == "" ? "" : "(-" + alias + ")") + "[=]<value>: " + description;
    }
};

std::shared_ptr<base_argument> make_argument(std::string long_name, std::string alias, std::string description, bool& value);
template<typename T>
inline std::shared_ptr<base_argument> make_argument(std::string long_name, std::string alias, std::string description, T& value) {
    return std::make_shared<value_argument<T>>(long_name, alias, description, value);
}

void parse_arguments(const std::vector<std::shared_ptr<base_argument>>& args, int argc, char* argv[]);

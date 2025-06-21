#pragma once

#include <cxxabi.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#ifdef ARGP_DEBUG
#include <iostream>
#endif

namespace argp {

struct read_error : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

// constants
const char collection_open = '[';
const char collection_close = ']';

// forward declarations
template<typename T>
inline T __parse_tuple(std::string_view in_str);
template<typename T>
inline T __parse_vector(std::string_view in_str);

// used in help() to print out what argument types are taken
template<typename T>
inline const std::string type_sig = abi::__cxa_demangle(typeid(T).name(), NULL, NULL, 0);
// specialised type signatures for some common types
template<>
inline const std::string type_sig<std::string> = "string";
// vector: {T,...,T}
template<typename K>
inline const std::string type_sig<std::vector<K>> = collection_open + type_sig<K> + ",...," + type_sig<K> + collection_close;
// tuple: {T1,T2,T3}
// partial is needed so we can get rid of the , at the end
template<typename... Ks>
inline const std::string __type_sig_partial = ((type_sig<Ks> + ",") + ...);
template<typename... Ks>
inline const std::string type_sig<std::tuple<Ks...>> = collection_open + __type_sig_partial<Ks...>.substr(0, __type_sig_partial<Ks...>.size() - 1) + collection_close;

template<typename T>
inline T __default_read(std::string_view in_str) {
    std::istringstream read_ss((std::string)in_str);
    T val;
    read_ss >> val;
    #ifdef ARGP_DEBUG
    std::cout << "[ARGP-D] using default read: stream state is " << read_ss.flags() << std::endl;
    #endif
    if (!read_ss.eof()) throw read_error("didn't EOF while reading value (" + (std::string)in_str + ")");
    if (!read_ss) throw read_error("error while reading value (" + (std::string)in_str + ")");
    return val;
}

template<typename T> struct is_container : std::false_type {};
template<typename... Args> struct is_container<std::tuple<Args...>> : std::true_type {};
template<typename K> struct is_container<std::vector<K>> : std::true_type {};

template<typename T> struct is_tuple : std::false_type {};
template<typename... Args> struct is_tuple<std::tuple<Args...>> : std::true_type {};

template<typename T> struct is_vector : std::false_type {};
template<typename K> struct is_vector<std::vector<K>> : std::true_type {};

template<typename T>
inline T parse_value(std::string_view in_str) {
    #ifdef ARGP_DEBUG
    std::cout << "[ARGP-D] parsing value `" << in_str << "` as " << type_sig<T> << std::endl;
    #endif
    if constexpr (is_tuple<T>::value) return __parse_tuple<T>(in_str);
    else if constexpr (is_vector<T>::value) return __parse_vector<T>(in_str);
    else return __default_read<T>(in_str);
}

template<>
inline bool parse_value<>(std::string_view in_str) {
    if (in_str.size() == 0) throw read_error("tried to parse empty string as bool");
    char firstc = tolower(in_str[0]);
    if (in_str == "true" || in_str == "yes" || firstc == 't' || firstc == 'y' || firstc == '+' || firstc == '1') {
        return true;
    } else if (in_str == "false" || in_str == "no" || firstc == 'f' || firstc == 'n' || firstc == '-' || firstc == '0') {
        return false;
    } else {
        throw read_error("invalid boolean value " + (std::string)in_str);
    }
}

// finds the separator that bounds this element in a container
inline std::size_t find_next_sep(std::string_view in_str, size_t prev_sep, bool find_nested = false) {
    if (in_str.size() - 1 <= prev_sep) return std::string::npos;

    #ifdef ARGP_DEBUG
    std::cout << "[ARGP-D] reading sep in `" << in_str.substr(prev_sep + 1) << "`" << std::endl;
    #endif

    size_t next_sep_pos = in_str.find(',', prev_sep + 1);
    if (next_sep_pos == std::string::npos) {
        next_sep_pos = in_str.rfind(collection_close, std::string::npos);
        if (next_sep_pos == std::string::npos || next_sep_pos <= prev_sep) throw read_error("container lacks closing bracket");
    }

    if (find_nested) {
        size_t n_read_pos = in_str.find(collection_open, prev_sep + 1);
        // we're reading a nested structure, there's a { before any other separator
        if (n_read_pos != std::string::npos && n_read_pos < next_sep_pos) {
            size_t b_level = 1;
            while (b_level != 0) {
                size_t i_open_pos = in_str.find(collection_open, n_read_pos + 1);
                size_t i_close_pos = in_str.find(collection_close, n_read_pos + 1);
                if (i_close_pos == std::string::npos) throw read_error("container nested structure lacks closing bracket");
                // find the closest bracket and see if we went deeper or shallower
                if (i_close_pos < i_open_pos || i_open_pos == std::string::npos) {
                    n_read_pos = i_close_pos;
                    --b_level;
                } else {
                    n_read_pos = i_open_pos;
                    ++b_level;
                }
                next_sep_pos = i_close_pos + 1;
            }
        }
    }
    return next_sep_pos;
}

// read a string into a tuple
template<typename T>
inline T __parse_tuple(std::string_view in_str) {
    if (in_str.empty()) throw read_error("tuple is empty");
    T read_tup;

    // read into the tuple
    std::apply ([&in_str](auto&... args) {
            std::size_t index = 0;
            // first separator is the opening {
            std::size_t prev_sep = 0;
            if (in_str[0] != collection_open) throw read_error("tuple lacks opening bracket");
            auto parse = [&index, &in_str, &prev_sep](auto& x) {
                // read until next separator
                size_t next_sep_pos = find_next_sep(in_str, prev_sep, is_container<__typeof__ x>::value);
                if (next_sep_pos == std::string::npos) throw read_error("found " + std::to_string(index) + " elements while reading tuple with " + std::to_string(sizeof...(args)) + " elements");
                #ifdef ARGP_DEBUG
                std::cout << "[ARGP-D] reading <" << type_sig<__typeof__ x> << "> from `" << in_str.substr(prev_sep + 1) << "`, next sep " << next_sep_pos << std::endl;
                #endif
                std::string_view r_view = in_str.substr(prev_sep + 1, next_sep_pos - prev_sep - 1);

                x = parse_value<__typeof__ x>(r_view);
                prev_sep = next_sep_pos;
                ++index;
            };
            (parse(args), ...);
            if (prev_sep != in_str.size() - 1) throw read_error("tuple has too many elements");
        }, read_tup
    );
    return read_tup;
}

// read a string into a vector
template<typename T>
inline T __parse_vector(std::string_view in_str) {
    if (in_str.empty()) throw read_error("vector is empty");
    T read_vec;

    // first separator is the opening {
    if (in_str[0] != collection_open) throw read_error("vector lacks opening bracket");

    std::size_t prev_sep = 0;
    while (true) {
        // read until next separator
        size_t next_sep_pos = find_next_sep(in_str, prev_sep, is_container<typename T::value_type>::value);
        if (next_sep_pos == std::string::npos)
            return read_vec;

        std::string_view r_view = in_str.substr(prev_sep + 1, next_sep_pos - prev_sep - 1);
        #ifdef ARGP_DEBUG
        std::cout << "[ARGP-D] reading <" << type_sig<typename T::value_type> << "> from `" << r_view << "`: prev " << prev_sep << " next sep " << next_sep_pos << std::endl;
        #endif

        read_vec.push_back(parse_value<typename T::value_type>(r_view));
        prev_sep = next_sep_pos;
    }
}

}

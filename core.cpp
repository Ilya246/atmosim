#include "gas.hpp"
#include "core.hpp"
#include <iostream>
#include <string>
#include <string_view>
#include <memory>
#include <limits>
#include <random>
#include <stdexcept>
#include <cctype>
#include "argparse/read.hpp"

using namespace std;

bool dyn_val::invalid() {
    return type == none_val || value_ptr == nullptr;
}

base_restriction::~base_restriction() {}

template <typename T>
num_restriction<T>::num_restriction(T* ptr, T min, T max)
    : value_ptr(ptr), min_value(min), max_value(max) {
    if (max_value < 0) {
        max_value = numeric_limits<T>::max();
    }
}

template <typename T>
bool num_restriction<T>::OK() {
    return *value_ptr >= min_value && *value_ptr <= max_value;
}

bool_restriction::bool_restriction(bool* ptr, bool target)
    : value_ptr(ptr), target_value(target) {}

bool bool_restriction::OK() {
    return *value_ptr == target_value;
}

// RNG implementation
uint32_t __xorshift_seed = rand();
uint32_t xorshift_rng() {
    uint32_t& x = __xorshift_seed;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}

float frand() {
    static random_device rd;
    static mt19937 gen(rd());
    static uniform_real_distribution<float> distribution;
    return distribution(gen);
}

// Utility functions
template <typename T>
T* get_dyn_ptr(dyn_val val) {
    return (T*)val.value_ptr;
}
template <typename T>
T& get_dyn(dyn_val val) {
    return *get_dyn_ptr<T>(val);
}

// istream overloads (declarations only, definitions elsewhere)
istream& operator>>(istream& stream, dyn_val& param);
istream& operator>>(istream& stream, shared_ptr<base_restriction>& restriction);

// Utility input/output helpers
istream& flush_stream(istream& stream) {
    stream.ignore(numeric_limits<streamsize>::max(), '\n');
    return stream;
}

template <typename T>
T get_input(const string& what, const string& invalid_err) {
    bool valid = false;
    T val;
    while (!valid) {
        valid = true;
        cout << what;
        cin >> val;
        if (cin.fail() || cin.peek() != '\n') {
            cerr << invalid_err;
            cin.clear();
            flush_stream(cin);
            valid = false;
        }
    }
    return val;
}

template int get_input<int>(const string&, const string&);
template float get_input<float>(const string&, const string&);
template string get_input<string>(const string&, const string&);

bool get_opt(const string& what, bool default_opt) {
    cout << what << (default_opt ? " [Y/n] " : " [y/N] ");
    if (flush_stream(cin).peek() == '\n') return default_opt;
    string opt;
    cin >> opt;
    char c = opt.empty() ? '\0' : tolower(opt[0]);
    if (c == 'y') return true;
    if (c == 'n') return false;
    return default_opt;
}

float pressure_temp_to_mols(float pressure, float temp) {
    extern float R, volume;
    return pressure * volume / (R * temp);
}

// Explicit template instantiations
// (needed for templates in .cpp)
template int* get_dyn_ptr<int>(dyn_val);
template float* get_dyn_ptr<float>(dyn_val);
template int& get_dyn<int>(dyn_val);
template float& get_dyn<float>(dyn_val);

istream& operator>>(istream& stream, shared_ptr<base_restriction>& restriction) {
    try {
        string all;
        stream >> all;
        string_view view(all);
        size_t cpos = view.find(',');
        string_view param_s = view.substr(1, cpos - 1);
        dyn_val param = argp::parse_value<dyn_val>(param_s);
        if (param.invalid()) {
            stream.setstate(ios_base::failbit);
            return stream;
        }
        switch (param.type) {
            case (int_val): {
                size_t cpos2 = view.find(',', cpos + 1);
                int restrA = argp::parse_value<int>(view.substr(cpos + 1, cpos2 - cpos - 1));
                int restrB = argp::parse_value<int>(view.substr(cpos2 + 1, view.size() - cpos2 - 2));
                restriction = make_shared<num_restriction<int>>(num_restriction<int>((int*)param.value_ptr, restrA, restrB));
                break;
            }
            case (float_val): {
                size_t cpos2 = view.find(',', cpos + 1);
                float restrA = argp::parse_value<float>(view.substr(cpos + 1, cpos2 - cpos - 1));
                float restrB = argp::parse_value<float>(view.substr(cpos2 + 1, view.size() - cpos2 - 2));
                restriction = make_shared<num_restriction<float>>((float*)param.value_ptr, restrA, restrB);
                break;
            }
            case (bool_val): {
                bool restr = argp::parse_value<bool>(view.substr(1, view.size() - 2));
                restriction = make_shared<bool_restriction>((bool*)param.value_ptr, restr);
                break;
            }
            default: {
                stream.setstate(ios_base::failbit);
                break;
            }
        }
    } catch (const argp::read_error& e) {
        stream.setstate(ios_base::failbit);
    }
    return stream;
}

template gas_type get_input<gas_type>(const std::string&, const std::string&); 
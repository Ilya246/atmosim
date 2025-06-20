#pragma once
#include <memory>
#include <string>
#include <istream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string_view>

// Value type enum
enum value_type {int_val, float_val, bool_val, none_val};

// Dynamic value struct
struct dyn_val {
    value_type type;
    void* value_ptr;
    bool invalid();
};

// Restriction base and derived types
struct base_restriction {
    virtual ~base_restriction();
    virtual bool OK() = 0;
};

template <typename T>
struct num_restriction : base_restriction {
    T* value_ptr;
    T min_value;
    T max_value;
    num_restriction(T* ptr, T min, T max);
    bool OK() override;
};

struct bool_restriction : base_restriction {
    bool* value_ptr;
    bool target_value;
    bool_restriction(bool* ptr, bool target);
    bool OK() override;
};

// RNG
typedef uint32_t rng_seed_t;
uint32_t xorshift_rng();
float frand();

// Utility functions
template <typename T>
T* get_dyn_ptr(dyn_val val);
template <typename T>
T& get_dyn(dyn_val val);

// istream overloads
// Remove generic template for operator>> to avoid ambiguity with std::string
std::istream& operator>>(std::istream&, dyn_val&);
std::istream& operator>>(std::istream&, std::shared_ptr<base_restriction>&);

// Utility input/output helpers
template <typename T>
T get_input(const std::string& what, const std::string& invalid_err = "Invalid value. Try again.\n");
bool get_opt(const std::string& what, bool default_opt = true);
float pressure_temp_to_mols(float pressure, float temp);
std::istream& flush_stream(std::istream& stream); 
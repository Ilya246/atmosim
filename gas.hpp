#pragma once
#include <string>
#include <vector>
#include <array>
#include <istream>

struct gas_type {
    int gas;
    float& amount() const;
    void update_amount(const float& delta, float& heat_capacity_cache);
    float& heat_cap() const;
    bool invalid() const;
    std::string name() const;
    bool operator==(const gas_type& other);
    bool operator!=(const gas_type& other);
};

extern const int gas_count;
extern float gas_amounts[];
extern float gas_heat_caps[];
extern const std::string gas_names[];
extern const int invalid_gas_num;
extern gas_type oxygen, nitrogen, plasma, tritium, water_vapour, carbon_dioxide, frezon, nitrous_oxide, nitrium, invalid_gas;
extern std::array<gas_type, 9> gases;

std::string list_gases();
bool is_gas(const std::string& gas);
gas_type to_gas(const std::string& gas);
gas_type parse_gas(const std::string& gas);
float get_heat_capacity();
float get_gas_mols();

std::istream& operator>>(std::istream& is, gas_type& g); 
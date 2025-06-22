#pragma once

#include <map>
#include <string>

#include <argparse/read.hpp>

#include "constants.hpp"

namespace asim {

/// <gas_type>

// a gas type definition
struct gas_type {
    float specific_heat;
    std::string name;

    gas_type(float specheat, std::string_view name): specific_heat(specheat), name(name) {};
    gas_type() = delete;
    gas_type(const gas_type& rhs) = delete;
};

extern inline const gas_type gases[];

// all supported gases - if it's not here, it's not supported
// UP TO DATE AS OF: 21.06.2025
inline const gas_type gases[] {
    {20.f  * heat_scale, "oxygen"},
    {30.f  * heat_scale, "nitrogen"},
    {200.f * heat_scale, "plasma"},
    {10.f  * heat_scale, "tritium"},
    {40.f  * heat_scale, "water_vapour"},
    {30.f  * heat_scale, "carbon_dioxide"},
    {600.f * heat_scale, "frezon"},
    {40.f  * heat_scale, "nitrous_oxide"},
    {10.f  * heat_scale, "nitrium"}
};

inline const size_t gas_count = std::end(gases) - std::begin(gases);

// for when we need to serialize a reference to a gas type
struct gas_ref {
    size_t idx = -1;

    float specific_heat() const {
        return gases[idx].specific_heat;
    }

    std::string_view name() const {
        return gases[idx].name;
    }

    bool operator==(const gas_ref& rhs) {
        return idx == rhs.idx;
    }
};

inline const std::map<std::string, gas_ref> string_gas_map = []() {
    std::map<std::string, gas_ref> map;
    for (size_t i = 0; i < gas_count; ++i) {
        map[gases[i].name] = {i};
    }
    return map;
}();

inline const gas_ref oxygen =         string_gas_map.at("oxygen");
inline const gas_ref nitrogen =       string_gas_map.at("nitrogen");
inline const gas_ref plasma =         string_gas_map.at("plasma");
inline const gas_ref tritium =        string_gas_map.at("tritium");
inline const gas_ref water_vapour =   string_gas_map.at("water_vapour");
inline const gas_ref carbon_dioxide = string_gas_map.at("carbon_dioxide");
inline const gas_ref frezon =         string_gas_map.at("frezon");
inline const gas_ref nitrous_oxide =  string_gas_map.at("nitrous_oxide");
inline const gas_ref nitrium =        string_gas_map.at("nitrium");

bool is_valid_gas(std::string_view name);

std::istream& operator>>(std::istream& stream, gas_ref& g);

std::string list_gases(std::string_view sep = ", ");

/// </gas_type>

/// <gas_mixture>

struct gas_mixture {
    float amounts[gas_count] {0.f};
    float temperature = T20C;
    float volume;

    gas_mixture(float volume): volume(volume) {};

    float amount_of(gas_ref gas) const;
    float total_gas() const;
    float heat_capacity() const;
    float heat_energy() const;
    float pressure() const;

    void set_amount_of(gas_ref gas, float to);
    void adjust_amount_of(gas_ref gas, float by);
    void adjust_pressure_of(gas_ref gas, float by);

    // fills gas mix to target pressure
    // NOTE: uses gas canister filling logic, will yield wrong pressure if filling non-empty mix
    void canister_fill_to(gas_ref gas, float temperature, float to_pressure);
    void canister_fill_to(gas_ref gas, float to_pressure);
    // NOTE: for optimisation purposes, this takes fractions and not ratios
    // if you want to use those with ratios, call get_fractions first
    void canister_fill_to(const std::vector<gas_ref>& gases, const std::vector<float>& fractions, float temperature, float to_pressure);
    void canister_fill_to(const std::vector<gas_ref>& gases, const std::vector<float>& fractions, float to_pressure);

    gas_mixture& operator+=(const gas_mixture& rhs);

    std::string to_string() const;

    // do gas reactions
    void reaction_tick();

private:
    void adjust_amount_of(gas_ref gas, float by, float&);

    // all supported reactions - if it's not here, it's not supported
    void react_plasma_fire(float&);
    void react_tritium_fire(float&);
    void react_N2O_decomposition(float&);
    void react_frezon_coolant(float&);
    void react_nitrium_decomposition(float&);
};

/// </gas_mixture>

/// <utility>

// function arguments should be in P,V,N,T order for consistency

float to_mols(float pressure, float volume, float temp);
float to_pressure(float volume, float mols, float temp);
float to_volume(float pressure, float mols, float temp);
// get temperature you would get after mixing 2 gases
float to_mix_temp(float lhs_c, float lhs_n, float lhs_t, float rhs_c, float rhs_n, float rhs_t);

// call with get_fractions() to get specific heat
float get_mix_heat_capacity(const std::vector<gas_ref>& gases, const std::vector<float>& amounts);

/// </utility>

}

template<>
inline std::string argp::type_sig<asim::gas_ref> = "gas";

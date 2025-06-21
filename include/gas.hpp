#pragma once

#include <map>
#include <string>

#include "constants.hpp"

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

extern const std::map<std::string, size_t> string_gas_map;

inline const size_t oxygen_idx =         string_gas_map.at("oxygen");
inline const size_t nitrogen_idx =       string_gas_map.at("nitrogen");
inline const size_t plasma_idx =         string_gas_map.at("plasma");
inline const size_t tritium_idx =        string_gas_map.at("tritium");
inline const size_t water_vapour_idx =   string_gas_map.at("water_vapour");
inline const size_t carbon_dioxide_idx = string_gas_map.at("carbon_dioxide");
inline const size_t frezon_idx =         string_gas_map.at("frezon");
inline const size_t nitrous_oxide_idx =  string_gas_map.at("nitrous_oxide");
inline const size_t nitrium_idx =        string_gas_map.at("nitrium");

inline const gas_type& oxygen =         gases[oxygen_idx];
inline const gas_type& nitrogen =       gases[nitrogen_idx];
inline const gas_type& plasma =         gases[plasma_idx];
inline const gas_type& tritium =        gases[tritium_idx];
inline const gas_type& water_vapour =   gases[water_vapour_idx];
inline const gas_type& carbon_dioxide = gases[carbon_dioxide_idx];
inline const gas_type& frezon =         gases[frezon_idx];
inline const gas_type& nitrous_oxide =  gases[nitrous_oxide_idx];
inline const gas_type& nitrium =        gases[nitrium_idx];

std::istream& operator>>(std::istream& stream, const gas_type*& g);

/// </gas_type>

/// <gas_mixture>

struct gas_mixture {
    float amounts[gas_count] {0.f};
    float temperature = T20C;
    const float volume;

    gas_mixture(float volume): volume(volume) {};

    float amount_of(size_t idx) const;
    // for `amount_of(oxygen)` syntax
    float amount_of(const gas_type& gas) const;

    void update_amount_of(size_t idx, float by);
    void update_amount_of(const gas_type& gas, float by);

    float total_gas() const;
    float heat_capacity() const;
    float pressure() const;

    // do gas reactions
    void reaction_tick();

private:
    void update_amount_of(size_t idx, float by, float&);
    void update_amount_of(const gas_type& gas, float by, float&);

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

/// </utility>

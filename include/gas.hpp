#include <map>
#include <string>

#include "constants.hpp"

struct gas_type {
    float specific_heat;
    std::string name;
};

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

inline const std::map<std::string_view, const gas_type&> string_gas_map;

inline const gas_type& oxygen =         gases[0];
inline const gas_type& nitrogen =       gases[1];
inline const gas_type& plasma =         gases[2];
inline const gas_type& tritium =        gases[3];
inline const gas_type& water_vapour =   gases[4];
inline const gas_type& carbon_dioxide = gases[5];
inline const gas_type& frezon =         gases[6];
inline const gas_type& nitrous_oxide =  gases[7];
inline const gas_type& nitrium =        gases[8];

inline const size_t gas_count = std::end(gases) - std::begin(gases);

struct gas_mixture {
    float amounts[gas_count] {0.f};
    float temperature = T20C;
};

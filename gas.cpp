#include "gas.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <cmath>
#include <array>
#include <istream>

using namespace std;

const int gas_count = 9;
float gas_amounts[gas_count] = {};
float gas_heat_caps[gas_count] = {20.f, 30.f, 200.f, 10.f, 40.f, 30.f, 600.f, 40.f, 10.f};
const string gas_names[gas_count] = {"oxygen", "nitrogen", "plasma", "tritium", "water_vapour", "carbon_dioxide", "frezon", "nitrous_oxide", "nitrium"};
const int invalid_gas_num = -1;

gas_type oxygen{0};
gas_type nitrogen{1};
gas_type plasma{2};
gas_type tritium{3};
gas_type water_vapour{4};
gas_type carbon_dioxide{5};
gas_type frezon{6};
gas_type nitrous_oxide{7};
gas_type nitrium{8};
gas_type invalid_gas{invalid_gas_num};
std::array<gas_type, 9> gases = {oxygen, nitrogen, plasma, tritium, water_vapour, carbon_dioxide, frezon, nitrous_oxide, nitrium};

unordered_map<string, gas_type> gas_map = {
    {"oxygen", oxygen},
    {"nitrogen", nitrogen},
    {"plasma", plasma},
    {"tritium", tritium},
    {"water_vapour", water_vapour},
    {"carbon_dioxide", carbon_dioxide},
    {"frezon", frezon},
    {"nitrous_oxide", nitrous_oxide},
    {"nitrium", nitrium}
};

float& gas_type::amount() const {
    return gas_amounts[gas];
}
void gas_type::update_amount(const float& delta, float& heat_capacity_cache) {
    amount() += delta;
    heat_capacity_cache += delta * heat_cap();
}
float& gas_type::heat_cap() const {
    return gas_heat_caps[gas];
}
bool gas_type::invalid() const {
    return gas == invalid_gas_num;
}
string gas_type::name() const {
    return gas_names[gas];
}
bool gas_type::operator==(const gas_type& other) {
    return gas == other.gas;
}
bool gas_type::operator!=(const gas_type& other) {
    return gas != other.gas;
}

string list_gases() {
    string out;
    for (gas_type g : gases) {
        out += g.name() + ", ";
    }
    out.resize(out.size() - 2);
    return out;
}

bool is_gas(const string& gas) {
    return gas_map.contains(gas);
}
gas_type to_gas(const string& gas) {
    if (!is_gas(gas)) {
        return invalid_gas;
    }
    return gas_map[gas];
}
gas_type parse_gas(const string& gas) {
    gas_type out = to_gas(gas);
    if (out.invalid()) {
        throw invalid_argument("Parsed invalid gas type.");
    }
    return out;
}
float get_heat_capacity() {
    float sum = 0.0;
    for (const gas_type& g : gases) {
        sum += g.amount() * g.heat_cap();
    }
    return sum;
}
float get_gas_mols() {
    float sum = 0.0;
    for (const gas_type& g : gases) {
        sum += g.amount();
    }
    return sum;
}

istream& operator>>(istream& is, gas_type& g) {
    string s;
    is >> s;
    g = parse_gas(s);
    return is;
} 
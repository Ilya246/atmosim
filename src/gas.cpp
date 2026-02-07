#include <map>
#include <numeric>
#include <stdexcept>
#include <string>

#include "gas.hpp"
#include "utility.hpp"

namespace asim {

/// <gas_type>

bool is_valid_gas(std::string_view name) {
    return string_gas_map.contains((std::string)name);
}

std::istream& operator>>(std::istream& stream, gas_ref& g) {
    std::string val;
    stream >> val;
    if (!is_valid_gas(val)) {
        stream.setstate(std::ios_base::failbit);
    } else {
        g = string_gas_map.at(val);
    }
    return stream;
}

std::string list_gases(std::string_view sep) {
    std::string out_str = gas_types[0].name;
    for (size_t i = 1; i < gas_count; ++i) {
        out_str += (std::string)sep + gas_types[i].name;
    }
    return out_str;
}

/// </gas_type>

/// <gas_mixture>

float gas_mixture::amount_of(gas_ref gas) const {
    return amounts[gas.idx];
}

float gas_mixture::total_gas() const {
    return std::accumulate(std::begin(amounts), std::end(amounts), 0.f);
}

float gas_mixture::heat_capacity() const {
    float sum = 0.f;
    for (size_t i = 0; i < gas_count; ++i) {
        gas_ref gas(i);
        sum += gas.specific_heat() * amount_of(gas);
    }
    return sum;
}

float gas_mixture::heat_energy() const {
    return heat_capacity() * temperature;
}

float gas_mixture::pressure() const {
    return total_gas() * temperature * rvol;
}

void gas_mixture::set_amount_of(gas_ref gas, float to) {
    amounts[gas.idx] = to;
}

void gas_mixture::adjust_amount_of(gas_ref gas, float by) {
    amounts[gas.idx] += by;
}

void gas_mixture::adjust_pressure_of(gas_ref gas, float by) {
    amounts[gas.idx] += to_mols(by, volume, temperature);
}

void gas_mixture::canister_fill_to(gas_ref gas, float temperature, float to_pressure) {
    gas_mixture fill_mix(volume);
    fill_mix.temperature = temperature;
    fill_mix.adjust_pressure_of(gas, to_pressure - pressure());

    *this += fill_mix;
}

void gas_mixture::canister_fill_to(gas_ref gas, float to_pressure) {
    canister_fill_to(gas, temperature, to_pressure);
}

void gas_mixture::canister_fill_to(const std::vector<gas_ref>& gases, const std::vector<float>& fractions, float temperature, float to_pressure) {
    CHECKEXCEPT {
        if (gases.size() != fractions.size()) throw std::runtime_error("amount of gases not equal to amount of fractions");
        if (std::abs(std::accumulate(fractions.begin(), fractions.end(), 0.f) - 1.f) > 0.001f) throw std::runtime_error("fractions did not sum up to 1");
    }
    gas_mixture fill_mix(volume);
    fill_mix.temperature = temperature;
    float delta_p = to_pressure - pressure();
    size_t gasc = gases.size();
    for (size_t i = 0; i < gasc; ++i) {
        fill_mix.adjust_pressure_of(gases[i], delta_p * fractions[i]);
    }

    *this += fill_mix;
}

void gas_mixture::canister_fill_to(const std::vector<gas_ref>& gases, const std::vector<float>& fractions, float to_pressure) {
    canister_fill_to(gases, fractions, temperature, to_pressure);
}

void gas_mixture::canister_fill_to(const std::vector<std::pair<gas_ref, float>>& gases, float temperature, float to_pressure) {
    gas_mixture fill_mix(volume);
    fill_mix.temperature = temperature;
    float delta_p = to_pressure - pressure();
    size_t gasc = gases.size();
    for (size_t i = 0; i < gasc; ++i) {
        fill_mix.adjust_pressure_of(gases[i].first, delta_p * gases[i].second);
    }

    *this += fill_mix;
}

void gas_mixture::canister_fill_to(const std::vector<std::pair<gas_ref, float>>& gases, float to_pressure) {
    canister_fill_to(gases, temperature, to_pressure);
}

gas_mixture& gas_mixture::operator+=(const gas_mixture& rhs) {
    float energy = heat_energy();
    for (size_t i = 0; i < gas_count; ++i) {
        gas_ref gas = {i};
        adjust_amount_of(gas, rhs.amount_of(gas));
    }
    temperature = (energy + rhs.heat_energy()) / heat_capacity();
    return *this;
}

std::string gas_mixture::to_string(char sep) const {
    std::string out_str;
    for (size_t i = 0; i < gas_count; ++i) {
        gas_ref gas = {i};
        float amt = amount_of(gas);
        if (amt > 0.f) {
            if (!out_str.empty()) out_str += sep;
            out_str += std::string(gas.name()) + " " + std::to_string(amt) + "mol";
        }
    }
    return out_str;
}

// UP TO DATE AS OF: 21.06.2025
bool gas_mixture::reaction_tick() {
    // calculating heat capacity is somewhat expensive, so cache it
    float heat_capacity_cache = heat_capacity();
    float temp = temperature; // original code caches temperature for some reason
    bool reacted = false;
    if (temp < frezon_production_temp && amount_of(oxygen) >= reaction_min_gas && amount_of(nitrogen) >= reaction_min_gas && amount_of(tritium) >= reaction_min_gas) {
        reacted |= react_frezon_production(heat_capacity_cache);
    }
    if (temp < nitrium_decomp_temp && amount_of(oxygen) >= reaction_min_gas && amount_of(nitrium) >= reaction_min_gas) {
        reacted |= react_nitrium_decomposition(heat_capacity_cache);
    }
    if (temp >= frezon_cool_temp && amount_of(nitrogen) >= reaction_min_gas && amount_of(frezon) >= reaction_min_gas) {
        reacted |= react_frezon_coolant(heat_capacity_cache);
    }
    if (temp >= n2o_decomp_temp && amount_of(nitrous_oxide) >= reaction_min_gas) {
        reacted |= react_N2O_decomposition(heat_capacity_cache);
    }
    if (temp >= trit_fire_temp && amount_of(oxygen) >= reaction_min_gas && amount_of(tritium) >= reaction_min_gas) {
        reacted |= react_tritium_fire(heat_capacity_cache);
    }
    if (temp >= plasma_fire_temp && amount_of(oxygen) >= reaction_min_gas && amount_of(plasma) >= reaction_min_gas) {
        reacted |= react_plasma_fire(heat_capacity_cache);
    }
    return reacted;
}

void gas_mixture::adjust_amount_of(gas_ref gas, float by, float& heat_capacity_cache) {
    heat_capacity_cache += gas.specific_heat() * by;
    amounts[gas.idx] += by;
}

// UP TO DATE AS OF: 21.06.2025
bool gas_mixture::react_plasma_fire(float& heat_capacity_cache) {
    float old_heat_capacity = heat_capacity_cache;
    float energy_released = 0.f;
    float temperature_scale = 0.f;
    if (temperature > plasma_upper_temperature) {
        temperature_scale = 1.f;
    } else {
        temperature_scale = (temperature - plasma_minimum_burn_temperature) / (plasma_upper_temperature - plasma_minimum_burn_temperature);
    }
    if (temperature_scale > 0.f) {
        float oxygen_burn_rate = oxygen_burn_rate_base - temperature_scale;
        float plasma_burn_rate = temperature_scale * (amount_of(oxygen) > amount_of(plasma) * plasma_oxygen_fullburn ? amount_of(plasma) / plasma_burn_rate_delta : amount_of(oxygen) / plasma_oxygen_fullburn / plasma_burn_rate_delta);
        if (plasma_burn_rate > minimum_heat_capacity) {
            plasma_burn_rate = std::min(plasma_burn_rate, std::min(amount_of(plasma), amount_of(oxygen) / oxygen_burn_rate));
            float supersaturation = std::min(1.f, std::max((amount_of(oxygen) / amount_of(plasma) - super_saturation_ends) / (super_saturation_threshold - super_saturation_ends), 0.f));

            adjust_amount_of(plasma, -plasma_burn_rate, heat_capacity_cache);

            adjust_amount_of(oxygen, -plasma_burn_rate * oxygen_burn_rate, heat_capacity_cache);

            float trit_delta = plasma_burn_rate * supersaturation;
            adjust_amount_of(tritium, trit_delta, heat_capacity_cache);

            float carbon_delta = plasma_burn_rate - trit_delta;
            adjust_amount_of(carbon_dioxide, carbon_delta, heat_capacity_cache);

            energy_released += fire_plasma_energy_released * plasma_burn_rate;
        }
    }
    if (heat_capacity_cache > minimum_heat_capacity) {
        temperature = (temperature * old_heat_capacity + energy_released) / heat_capacity_cache;
    }
    return energy_released > 0.f;
}

// UP TO DATE AS OF: 21.06.2025
bool gas_mixture::react_tritium_fire(float& heat_capacity_cache) {
    float old_heat_capacity = heat_capacity_cache;
    float energy_released = 0.f;
    float burned_fuel = 0.f;
    if (amount_of(oxygen) < amount_of(tritium) || minimum_tritium_oxyburn_energy > temperature * heat_capacity_cache) {
        burned_fuel = std::min(amount_of(tritium), amount_of(oxygen) / tritium_burn_oxy_factor);
        float trit_delta = -burned_fuel;
        adjust_amount_of(tritium, trit_delta, heat_capacity_cache);
    } else {
        burned_fuel = amount_of(tritium);
        float trit_delta = -amount_of(tritium) / tritium_burn_trit_factor;

        adjust_amount_of(tritium, trit_delta, heat_capacity_cache);
        adjust_amount_of(oxygen, -amount_of(tritium), heat_capacity_cache);

        energy_released += fire_hydrogen_energy_released * burned_fuel * (tritium_burn_trit_factor - 1.f);
    }
    if (burned_fuel > 0.f) {
        energy_released += fire_hydrogen_energy_released * burned_fuel;

        adjust_amount_of(water_vapour, burned_fuel, heat_capacity_cache);
    }
    if (heat_capacity_cache > minimum_heat_capacity) {
        temperature = (temperature * old_heat_capacity + energy_released) / heat_capacity_cache;
    }
    return burned_fuel > 0.f;
}

// UP TO DATE AS OF: 21.06.2025
bool gas_mixture::react_N2O_decomposition(float& heat_capacity_cache) {
    float n2o = amount_of(nitrous_oxide);
    float burned_fuel = n2o * N2Odecomposition_rate;
    adjust_amount_of(nitrous_oxide, -burned_fuel, heat_capacity_cache);
    adjust_amount_of(nitrogen, burned_fuel, heat_capacity_cache);
    adjust_amount_of(oxygen, burned_fuel * 0.5f, heat_capacity_cache);
    // does not update temperature - this is accurate to the source
    return burned_fuel > 0.f;
}

// UP TO DATE AS OF: 29.06.2025
bool gas_mixture::react_frezon_production(float& heat_capacity_cache) {
    float efficiency = temperature / frezon_production_max_efficiency_temperature;
    float loss = 1.f - efficiency;

    float catalyst_limit = amount_of(nitrogen) * (frezon_production_nitrogen_ratio / efficiency);
    float oxy_limit = std::min(amount_of(oxygen), catalyst_limit) / frezon_production_trit_ratio;

    float trit_burned = std::min(oxy_limit, amount_of(tritium));
    float oxy_burned = trit_burned * frezon_production_trit_ratio;

    float oxy_conversion = oxy_burned / frezon_production_conversion_rate;
    float trit_conversion = trit_burned / frezon_production_conversion_rate;
    float total = oxy_conversion + trit_conversion;

    adjust_amount_of(oxygen, -oxy_conversion, heat_capacity_cache);
    adjust_amount_of(tritium, -trit_conversion, heat_capacity_cache);
    adjust_amount_of(frezon, total * efficiency, heat_capacity_cache);
    adjust_amount_of(nitrogen, total * loss, heat_capacity_cache);

    return true;
}

// UP TO DATE AS OF: 21.06.2025
bool gas_mixture::react_frezon_coolant(float& heat_capacity_cache) {
    float old_heat_capacity = heat_capacity_cache;
    float energy_modifier = 1.f;
    float scale = (temperature - frezon_cool_lower_temperature) / (frezon_cool_mid_temperature - frezon_cool_lower_temperature);
    if (scale > 1.f) {
        energy_modifier = std::min(scale, frezon_cool_maximum_energy_modifier);
        scale = 1.f;
    }
    float burn_rate = amount_of(frezon) * scale / frezon_cool_rate_modifier;
    float energy_released = 0.f;
    if (burn_rate > minimum_heat_capacity) {
        float nit_delta = -std::min(burn_rate * frezon_nitrogen_cool_ratio, amount_of(nitrogen));
        float frezon_delta = -std::min(burn_rate, amount_of(frezon));

        adjust_amount_of(nitrogen, nit_delta, heat_capacity_cache);
        adjust_amount_of(frezon, frezon_delta, heat_capacity_cache);
        adjust_amount_of(nitrous_oxide, -nit_delta - frezon_delta, heat_capacity_cache);

        energy_released = burn_rate * frezon_cool_energy_released * energy_modifier;
    }
    if (heat_capacity_cache > minimum_heat_capacity) {
        temperature = (temperature * old_heat_capacity + energy_released) / heat_capacity_cache;
    }
    return energy_released > 0.f;
}

// UP TO DATE AS OF: 21.06.2025
bool gas_mixture::react_nitrium_decomposition(float& heat_capacity_cache) {
    float efficiency = std::min(temperature / 2984.f, amount_of(nitrium));

    if (amount_of(nitrium) - efficiency < 0.f)
        return false;

    adjust_amount_of(nitrium, -efficiency, heat_capacity_cache);
    adjust_amount_of(water_vapour, efficiency, heat_capacity_cache);
    adjust_amount_of(nitrogen, efficiency, heat_capacity_cache);

    float energy_released = efficiency * nitrium_decomposition_energy;
    if (heat_capacity_cache > minimum_heat_capacity) {
        temperature = (temperature * heat_capacity_cache + energy_released) / heat_capacity_cache;
    }
    return energy_released > 0.f;
}

/// </gas_mixture>

/// <utility>

float to_mols(float pressure, float volume, float temp) {
    return pressure*volume / (R*temp);
}

float to_pressure(float volume, float mols, float temp) {
    return mols*R*temp / volume;
}

float to_volume(float pressure, float mols, float temp) {
    return mols*R*temp / pressure;
}

float to_mix_temp(float lhs_c, float lhs_n, float lhs_t, float rhs_c, float rhs_n, float rhs_t) {
    float lhs_C = lhs_c * lhs_n, rhs_C = rhs_c * rhs_n;
    return (lhs_C * lhs_t + rhs_C * rhs_t) / (lhs_C + rhs_C);
}

float get_mix_heat_capacity(const std::vector<gas_ref>& gases, const std::vector<float>& amounts) {
    float total_heat_cap = 0.f;
    size_t ct = gases.size();
    for(size_t i = 0; i < ct; ++i) {
        total_heat_cap += gases[i].specific_heat() * amounts[i];
    }
    return total_heat_cap;
}

/// </utility>

}

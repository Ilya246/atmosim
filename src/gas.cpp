#include <map>
#include <numeric>
#include <string>

#include "gas.hpp"

/// <gas_type>

std::istream& operator>>(std::istream& stream, gas_ref& g) {
    std::string val;
    stream >> val;
    if (string_gas_map.count(val) == 0) {
        stream.setstate(std::ios_base::failbit);
    } else {
        g = string_gas_map.at(val);
    }
    return stream;
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
        gas_ref gas = {i};
        sum += gas.specific_heat() * amount_of(gas);
    }
    return sum;
}

float gas_mixture::heat_energy() const {
    return heat_capacity() * temperature;
}

float gas_mixture::pressure() const {
    return total_gas() * R * temperature / volume;
}

void gas_mixture::adjust_amount_of(gas_ref gas, float by) {
    amounts[gas.idx] += by;
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

// UP TO DATE AS OF: 21.06.2025
void gas_mixture::reaction_tick() {
    // calculating heat capacity is somewhat expensive, so cache it
    float heat_capacity_cache = heat_capacity();
    if (temperature < nitrium_decomp_temp && amount_of(oxygen) >= reaction_min_gas && amount_of(nitrium) >= reaction_min_gas) {
        react_nitrium_decomposition(heat_capacity_cache);
    }
    if (temperature >= frezon_cool_temp && amount_of(nitrogen) >= reaction_min_gas && amount_of(frezon) >= reaction_min_gas) {
        react_frezon_coolant(heat_capacity_cache);
    }
    if (temperature >= n2o_decomp_temp && amount_of(nitrous_oxide) >= reaction_min_gas) {
        react_N2O_decomposition(heat_capacity_cache);
    }
    if (amount_of(oxygen) >= reaction_min_gas && temperature >= fire_temp) {
        if (amount_of(tritium) >= reaction_min_gas) {
            react_tritium_fire(heat_capacity_cache);
        }
        if (amount_of(plasma) >= reaction_min_gas) {
            react_plasma_fire(heat_capacity_cache);
        }
    }
}

void gas_mixture::adjust_amount_of(gas_ref gas, float by, float& heat_capacity_cache) {
    heat_capacity_cache += gas.specific_heat() * by;
    amounts[gas.idx] += by;
}

// UP TO DATE AS OF: 21.06.2025
void gas_mixture::react_plasma_fire(float& heat_capacity_cache) {
    float old_heat_capacity = heat_capacity_cache;
    float energy_released = 0.f;
    float temperature_scale = 0.f;
    if (temperature > plasma_upper_temperature) {
        temperature_scale = 1.f;
    } else {
        temperature_scale = (temperature - fire_temp) / (plasma_upper_temperature - fire_temp);
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
}

// UP TO DATE AS OF: 21.06.2025
void gas_mixture::react_tritium_fire(float& heat_capacity_cache) {
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
}

// UP TO DATE AS OF: 21.06.2025
void gas_mixture::react_N2O_decomposition(float& heat_capacity_cache) {
    float old_heat_capacity = heat_capacity_cache;
    float n2o = amount_of(nitrous_oxide);
    float burned_fuel = n2o * N2Odecomposition_rate;
    adjust_amount_of(nitrous_oxide, -burned_fuel, heat_capacity_cache);
    adjust_amount_of(nitrogen, burned_fuel, heat_capacity_cache);
    adjust_amount_of(oxygen, burned_fuel * 0.5f, heat_capacity_cache);
    temperature *= old_heat_capacity / heat_capacity_cache;
}

// UP TO DATE AS OF: 21.06.2025
void gas_mixture::react_frezon_coolant(float& heat_capacity_cache) {
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
}

// UP TO DATE AS OF: 21.06.2025
void gas_mixture::react_nitrium_decomposition(float& heat_capacity_cache) {
        float efficiency = std::min(temperature / 2984.f, amount_of(nitrium));

        if (amount_of(nitrium) - efficiency < 0.f)
            return;

        adjust_amount_of(nitrium, -efficiency, heat_capacity_cache);
        adjust_amount_of(water_vapour, efficiency, heat_capacity_cache);
        adjust_amount_of(nitrogen, efficiency, heat_capacity_cache);

        float energy_released = efficiency * nitrium_decomposition_energy;
        if (heat_capacity_cache > minimum_heat_capacity) {
            temperature = (temperature * heat_capacity_cache + energy_released) / heat_capacity_cache;
        }
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

/// </utility>

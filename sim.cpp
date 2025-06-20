#include "sim.hpp"
#include "core.hpp"
#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <array>
#include "gas.hpp"
#include "tank.hpp"

using namespace std;

void do_plasma_fire() {
    float old_heat_capacity = heat_capacity_cache;
    float energy_released = 0.0;
    float temperature_scale = 0.0;
    if (temperature > plasma_upper_temperature) {
        temperature_scale = 1.0;
    } else {
        temperature_scale = (temperature - fire_temp) / (plasma_upper_temperature - fire_temp);
    }
    if (temperature_scale > 0) {
        float oxygen_burn_rate = oxygen_burn_rate_base - temperature_scale;
        float plasma_burn_rate = temperature_scale * (oxygen.amount() > plasma.amount() * plasma_oxygen_fullburn ? plasma.amount() / plasma_burn_rate_delta : oxygen.amount() / plasma_oxygen_fullburn / plasma_burn_rate_delta);
        if (plasma_burn_rate > minimum_heat_capacity) {
            plasma_burn_rate = std::min(plasma_burn_rate, std::min(plasma.amount(), oxygen.amount() / oxygen_burn_rate));
            float supersaturation = std::min(1.0f, std::max((oxygen.amount() / plasma.amount() - super_saturation_ends) / (super_saturation_threshold - super_saturation_ends), 0.0f));

            plasma.update_amount(-plasma_burn_rate, heat_capacity_cache);
            oxygen.update_amount(-plasma_burn_rate * oxygen_burn_rate, heat_capacity_cache);
            float trit_delta = plasma_burn_rate * supersaturation;
            tritium.update_amount(trit_delta, heat_capacity_cache);
            float carbon_delta = plasma_burn_rate - trit_delta;
            carbon_dioxide.update_amount(carbon_delta, heat_capacity_cache);
            energy_released += fire_plasma_energy_released * plasma_burn_rate;
        }
    }
    if (heat_capacity_cache > minimum_heat_capacity) {
        temperature = (temperature * old_heat_capacity + energy_released) / heat_capacity_cache;
    }
}

void do_trit_fire() {
    float old_heat_capacity = heat_capacity_cache;
    float energy_released = 0.f;
    float burned_fuel = 0.f;
    if (oxygen.amount() < tritium.amount() || minimum_tritium_oxyburn_energy > temperature * heat_capacity_cache) {
        burned_fuel = std::min(tritium.amount(), oxygen.amount() / tritium_burn_oxy_factor);
        float trit_delta = -burned_fuel;
        tritium.update_amount(trit_delta, heat_capacity_cache);
    } else {
        burned_fuel = tritium.amount();
        float trit_delta = -tritium.amount() / tritium_burn_trit_factor;
        tritium.update_amount(trit_delta, heat_capacity_cache);
        oxygen.update_amount(-tritium.amount(), heat_capacity_cache);
        energy_released += fire_hydrogen_energy_released * burned_fuel * (tritium_burn_trit_factor - 1.f);
    }
    if (burned_fuel > 0.f) {
        energy_released += fire_hydrogen_energy_released * burned_fuel;
        water_vapour.update_amount(burned_fuel, heat_capacity_cache);
    }
    if (heat_capacity_cache > minimum_heat_capacity) {
        temperature = (temperature * old_heat_capacity + energy_released) / heat_capacity_cache;
    }
}

void doN2ODecomposition() {
    float old_heat_capacity = heat_capacity_cache;
    float& n2o = nitrous_oxide.amount();
    float burned_fuel = n2o * N2Odecomposition_rate;
    nitrous_oxide.update_amount(-burned_fuel, heat_capacity_cache);
    nitrogen.update_amount(burned_fuel, heat_capacity_cache);
    oxygen.update_amount(burned_fuel * 0.5f, heat_capacity_cache);
    temperature *= old_heat_capacity / heat_capacity_cache;
}

void do_frezon_coolant() {
    float old_heat_capacity = heat_capacity_cache;
    float energy_modifier = 1.f;
    float scale = (temperature - frezon_cool_lower_temperature) / (frezon_cool_mid_temperature - frezon_cool_lower_temperature);
    if (scale > 1.f) {
        energy_modifier = std::min(scale, frezon_cool_maximum_energy_modifier);
        scale = 1.f;
    }
    float burn_rate = frezon.amount() * scale / frezon_cool_rate_modifier;
    float energy_released = 0.f;
    if (burn_rate > minimum_heat_capacity) {
        float nit_delta = -std::min(burn_rate * frezon_nitrogen_cool_ratio, nitrogen.amount());
        float frezon_delta = -std::min(burn_rate, frezon.amount());
        nitrogen.update_amount(nit_delta, heat_capacity_cache);
        frezon.update_amount(frezon_delta, heat_capacity_cache);
        nitrous_oxide.update_amount(-nit_delta - frezon_delta, heat_capacity_cache);
        energy_released = burn_rate * frezon_cool_energy_released * energy_modifier;
    }
    if (heat_capacity_cache > minimum_heat_capacity) {
        temperature = (temperature * old_heat_capacity + energy_released) / heat_capacity_cache;
    }
}

void do_nitrium_decomposition() {
    float efficiency = std::min(temperature / 2984.0f, nitrium.amount());
    if (nitrium.amount() - efficiency < 0)
        return;
    nitrium.update_amount(-efficiency, heat_capacity_cache);
    water_vapour.update_amount(efficiency, heat_capacity_cache);
    nitrogen.update_amount(efficiency, heat_capacity_cache);
    float energy_released = efficiency * nitrium_decomposition_energy;
    if (heat_capacity_cache > minimum_heat_capacity) {
        temperature = (temperature * heat_capacity_cache + energy_released) / heat_capacity_cache;
    }
}

void react() {
    heat_capacity_cache = get_heat_capacity();
    if (temperature < nitrium_decomp_temp && oxygen.amount() >= 0.01f && nitrium.amount() >= 0.01f) {
        do_nitrium_decomposition();
    }
    if (temperature >= frezon_cool_temp && nitrogen.amount() >= 0.01f && frezon.amount() >= 0.01f) {
        do_frezon_coolant();
    }
    if (temperature >= n2o_decomp_temp && nitrous_oxide.amount() >= 0.01f) {
        doN2ODecomposition();
    }
    if (oxygen.amount() >= 0.01f && temperature >= fire_temp) {
        if (tritium.amount() >= 0.01f) {
            do_trit_fire();
        }
        if (plasma.amount() >= 0.01f) {
            do_plasma_fire();
        }
    }
}

void loop(int n) {
    while (tick < n) {
        react();
        ++tick;
    }
}

void loop() {
    if (!check_status) {
        loop(tick_cap);
        return;
    }
    while (tick < tick_cap && cur_state == intact) {
        react();
        tank_check_status();
        ++tick;
    }
}

void loop_print() {
    while (tick < tick_cap && cur_state == intact) {
        react();
        tank_check_status();
        ++tick;
        status();
    }
}

void full_input_setup() {
    float sumheat = 0.0;
    while (true) {
        cout << "Available gases: " << list_gases() << endl;
        gas_type gas = get_input<gas_type>("Enter gas to add: ");
        float moles = get_input<float>("Enter moles: ");
        float temp = get_input<float>("Enter temperature: ");
        sumheat += temp * gas.heat_cap() * moles;
        gas.amount() += moles;
        if (!get_opt("Continue?")) {
            break;
        }
    }
    temperature = sumheat / get_heat_capacity();
}

float get_gasmix_spec_heat(const vector<gas_type>& gases, const vector<float>& ratios) {
    float total_heat_cap = 0;
    float total_ratio = 0;
    for(size_t i = 0; i < gases.size(); ++i) {
        total_heat_cap += gases[i].heat_cap() * ratios[i];
        total_ratio += ratios[i];
    }
    if (total_ratio == 0) return 0;
    return total_heat_cap / total_ratio;
}

float mix_input_setup(const vector<gas_type>& mix_gases, const vector<float>& mix_ratios, const vector<gas_type>& primer_gases, const vector<float>& primer_ratios, float fuel_temp, float primer_temp, float target_temp) {
    float fuel_specheat = get_gasmix_spec_heat(mix_gases, mix_ratios);
    float primer_specheat = get_gasmix_spec_heat(primer_gases, primer_ratios);
    if (primer_specheat == 0) return -1.0;
    float fuel_pressure = (target_temp / primer_temp - 1.0) * pressure_cap / (fuel_specheat / primer_specheat - 1.0 + target_temp * (1.0 / primer_temp - fuel_specheat / primer_specheat / fuel_temp));
    float fuel_mols = pressure_temp_to_mols(fuel_pressure, fuel_temp);
    float total_mix_ratio = 0;
    for(float r : mix_ratios) total_mix_ratio += r;
    if(total_mix_ratio > 0) {
        for(size_t i = 0; i < mix_gases.size(); ++i) {
            mix_gases[i].amount() = fuel_mols * mix_ratios[i] / total_mix_ratio;
        }
    }
    float primer_mols = pressure_temp_to_mols(pressure_cap - fuel_pressure, primer_temp);
    float total_primer_ratio = 0;
    for(float r : primer_ratios) total_primer_ratio += r;
    if(total_primer_ratio > 0) {
        for(size_t i = 0; i < primer_gases.size(); ++i) {
            primer_gases[i].amount() = primer_mols * primer_ratios[i] / total_primer_ratio;
        }
    }
    temperature = (fuel_mols * fuel_specheat * fuel_temp + primer_mols * primer_specheat * primer_temp) / (fuel_mols * fuel_specheat + primer_mols * primer_specheat);
    return fuel_pressure;
}

void known_input_setup(const vector<gas_type>& mix_gases, const vector<float>& mix_ratios, const vector<gas_type>& primer_gases, const vector<float>& primer_ratios, float fuel_temp, float primer_temp, float fuel_pressure) {
    float fuel_specheat = get_gasmix_spec_heat(mix_gases, mix_ratios);
    float primer_specheat = get_gasmix_spec_heat(primer_gases, primer_ratios);
    float fuel_mols = pressure_temp_to_mols(fuel_pressure, fuel_temp);
    float total_mix_ratio = 0;
    for(float r : mix_ratios) total_mix_ratio += r;
    if(total_mix_ratio > 0) {
        for(size_t i = 0; i < mix_gases.size(); ++i) {
            mix_gases[i].amount() = fuel_mols * mix_ratios[i] / total_mix_ratio;
        }
    }
    float primer_mols = pressure_temp_to_mols(pressure_cap - fuel_pressure, primer_temp);
    float total_primer_ratio = 0;
    for(float r : primer_ratios) total_primer_ratio += r;
    if(total_primer_ratio > 0) {
        for(size_t i = 0; i < primer_gases.size(); ++i) {
            primer_gases[i].amount() = primer_mols * primer_ratios[i] / total_primer_ratio;
        }
    }
    temperature = (fuel_mols * fuel_specheat * fuel_temp + primer_mols * primer_specheat * primer_temp) / (fuel_mols * fuel_specheat + primer_mols * primer_specheat);
}

float unimix_input_setup(gas_type gas1, gas_type gas2, float temp1, float temp2, float target_temp) {
    float fuel_pressure = (target_temp / temp2 - 1.0) * pressure_cap / (gas1.heat_cap() / gas2.heat_cap() - 1.0 + target_temp * (1.0 / temp2 - gas1.heat_cap() / gas2.heat_cap() / temp1));
    gas1.amount() = pressure_temp_to_mols(fuel_pressure, temp1);
    gas2.amount() = pressure_temp_to_mols(pressure_cap - fuel_pressure, temp2);
    temperature = (gas1.amount() * temp1 * gas1.heat_cap() + gas2.amount() * temp2 * gas2.heat_cap()) / (gas1.amount() * gas1.heat_cap() + gas2.amount() * gas2.heat_cap());
    return fuel_pressure;
}

void unimix_to_input_setup(gas_type gas1, gas_type gas2, float temp, float second_per_first) {
    temperature = temp;
    float total = pressure_temp_to_mols(pressure_cap, temperature);
    gas1.amount() = total / (1.0 + second_per_first);
    gas2.amount() = total - gas1.amount();
}

void heat_cap_input_setup() {
    cout << "Enter heat capacities for " << list_gases() << ": ";
    for (gas_type g : gases) {
        cin >> g.heat_cap();
    }
} 
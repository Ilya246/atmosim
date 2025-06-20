#include "tank.hpp"
#include <cmath>
#include <iostream>
#include <vector>
#include <array>

using namespace std;

float temperature = 293.15, volume = 5.0, pressure_cap = 1013.25, pipe_pressure_cap = 4500.0, required_transfer_volume = 1400.0,
radius = 0.0,
leaked_heat = 0.0;
tank_state cur_state = intact;
int integrity = 3, leak_count = 0, tick = 0,
tick_cap = 60, pipe_tick_cap = 1000,
log_level = 1;
bool step_target_temp = false,
check_status = true,
simple_output = false, silent = false,
optimise_int = false, optimise_maximise = true, optimise_before = false;
float TCMB = 2.7, T0C = 273.15, T20C = 293.15,
fire_temp = 373.15, minimum_heat_capacity = 0.0003, one_atmosphere = 101.325, R = 8.314462618,
tank_leak_pressure = 30.0 * one_atmosphere, tank_rupture_pressure = 40.0 * one_atmosphere, tank_fragment_pressure = 50.0 * one_atmosphere, tank_fragment_scale = 2.0 * one_atmosphere,
fire_hydrogen_energy_released = 284000.0, minimum_tritium_oxyburn_energy = 143000.0, tritium_burn_oxy_factor = 100.0, tritium_burn_trit_factor = 10.0,
fire_plasma_energy_released = 160000.0, super_saturation_threshold = 96.0, super_saturation_ends = super_saturation_threshold / 3.0, oxygen_burn_rate_base = 1.4, plasma_upper_temperature = 1643.15, plasma_oxygen_fullburn = 10.0, plasma_burn_rate_delta = 9.0,
n2o_decomp_temp = 850.0, N2Odecomposition_rate = 0.5,
frezon_cool_temp = 23.15, frezon_cool_lower_temperature = 23.15, frezon_cool_mid_temperature = 373.15, frezon_cool_maximum_energy_modifier = 10.0, frezon_cool_rate_modifier = 20.0, frezon_nitrogen_cool_ratio = 5.0, frezon_cool_energy_released = -600000.0,
nitrium_decomp_temp = T0C + 70.0, nitrium_decomposition_energy = 30000.0,
tickrate = 0.5,
lower_target_temp = fire_temp + 0.1, temperature_step = 1.002, temperature_step_min = 0.05, ratio_step = 1.005, ratio_bounds = 20.0,
max_runtime = 3.0, bounds_scale = 0.5, stepping_scale = 0.75,
heat_capacity_cache = 0.0;
vector<gas_type> active_gases;
size_t sample_rounds = 3;

void reset() {
    for (const gas_type& g : gases) {
        g.amount() = 0.0;
    }
    temperature = 293.15;
    cur_state = intact;
    integrity = 3;
    tick = 0;
    leak_count = 0;
    radius = 0.0;
    leaked_heat = 0.0;
}

float get_pressure() {
    return get_gas_mols() * R * temperature / volume;
}

float get_cur_range() {
    return sqrt((get_pressure() - tank_fragment_pressure) / tank_fragment_scale);
}

void tank_check_status() {
    float pressure = get_pressure();
    if (pressure > tank_leak_pressure) {
        if (pressure > tank_rupture_pressure) {
            if (pressure > tank_fragment_pressure) {
                for (int i = 0; i < 3; ++i) {
                    // react(); // Will be called from sim.cpp
                }
                cur_state = exploded;
                radius = get_cur_range();
                for (const gas_type& g : gases) {
                    leaked_heat += g.amount() * g.heat_cap() * temperature;
                }
                return;
            }
            if (integrity <= 0) {
                cur_state = ruptured;
                radius = 0.0;
                for (const gas_type& g : gases) {
                    leaked_heat += g.amount() * g.heat_cap() * temperature;
                }
                return;
            }
            integrity--;
            return;
        }
        if (integrity <= 0) {
            for (const gas_type& g : gases) {
                leaked_heat += g.amount() * g.heat_cap() * temperature * 0.25;
                g.amount() *= 0.75;
            }
            leak_count++;
        } else {
            integrity--;
        }
        return;
    }
    if (integrity < 3) {
        integrity++;
    }
}

void status() {
    cout << "TICK: " << tick << " || Status: pressure " << get_pressure() << "kPa \\ integrity " << integrity << " \\ temperature " << temperature << "K\n_contents: ";
    for (const gas_type& g : gases) {
        cout << g.name() << ": " << g.amount() << " mol; ";
    }
    cout << endl;
    if (cur_state == exploded) {
        cout << "EXPLOSION: range " << get_cur_range() << endl;
    } else if (cur_state == ruptured) {
        cout << "RUPTURED" << endl;
    }
} 
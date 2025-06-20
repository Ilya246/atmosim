#pragma once
#include <vector>
#include <string>
#include <array>
#include "gas.hpp"

// Tank state enum
enum tank_state {
    intact = 0,
    ruptured = 1,
    exploded = 2
};

extern float temperature, volume, pressure_cap, pipe_pressure_cap, required_transfer_volume, radius, leaked_heat;
extern tank_state cur_state;
extern int integrity, leak_count, tick, tick_cap, pipe_tick_cap, log_level;
extern bool step_target_temp, check_status, simple_output, silent, optimise_int, optimise_maximise, optimise_before;
extern float TCMB, T0C, T20C, fire_temp, minimum_heat_capacity, one_atmosphere, R, tank_leak_pressure, tank_rupture_pressure, tank_fragment_pressure, tank_fragment_scale, fire_hydrogen_energy_released, minimum_tritium_oxyburn_energy, tritium_burn_oxy_factor, tritium_burn_trit_factor, fire_plasma_energy_released, super_saturation_threshold, super_saturation_ends, oxygen_burn_rate_base, plasma_upper_temperature, plasma_oxygen_fullburn, plasma_burn_rate_delta, n2o_decomp_temp, N2Odecomposition_rate, frezon_cool_temp, frezon_cool_lower_temperature, frezon_cool_mid_temperature, frezon_cool_maximum_energy_modifier, frezon_cool_rate_modifier, frezon_nitrogen_cool_ratio, frezon_cool_energy_released, nitrium_decomp_temp, nitrium_decomposition_energy, tickrate, lower_target_temp, temperature_step, temperature_step_min, ratio_step, ratio_bounds, max_runtime, bounds_scale, stepping_scale, heat_capacity_cache;
extern std::vector<gas_type> active_gases;
extern size_t sample_rounds;

void reset();
float get_pressure();
float get_cur_range();
void tank_check_status();
void status(); 
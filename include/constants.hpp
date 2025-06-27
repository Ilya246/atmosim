#pragma once

#include <chrono>

namespace asim {

inline const size_t LOG_NONE = 0, LOG_BASIC = 1, LOG_INFO = 2, LOG_DEBUG = 3, LOG_TRACE = 4;

inline const float heat_scale = 1.0 / 8.0,

pressure_cap = 1013.25f,
pipe_pressure_cap = 4500.f,
required_transfer_volume = 1400.f,
TCMB = 2.7f,
T0C = 273.15f,
T20C = 293.15f,
fire_temp = T0C + 100.f,

minimum_heat_capacity = 0.0003,
R = 8.314462618f,

one_atmosphere = 101.325f,

tank_leak_pressure = 30.f * one_atmosphere,
tank_rupture_pressure = 40.f * one_atmosphere,
tank_fragment_pressure = 50.f * one_atmosphere,
tank_fragment_scale = 2.f * one_atmosphere,
tank_volume = 5.f,

reaction_min_gas = 0.01f,

fire_hydrogen_energy_released = 284000.f * heat_scale,
minimum_tritium_oxyburn_energy = 143000.f * heat_scale,
tritium_burn_oxy_factor = 100.f,
tritium_burn_trit_factor = 10.f,

fire_plasma_energy_released = 160000.f * heat_scale,
super_saturation_threshold = 96.f,
super_saturation_ends = super_saturation_threshold / 3.f,
oxygen_burn_rate_base = 1.4f,
plasma_upper_temperature = 1370.f + T0C,
plasma_oxygen_fullburn = 10.f,
plasma_burn_rate_delta = 9.f,

n2o_decomp_temp = 850.f,
N2Odecomposition_rate = 0.5f,

frezon_cool_temp = 23.15f,
frezon_cool_lower_temperature = 23.15f,
frezon_cool_mid_temperature = 373.15f,
frezon_cool_maximum_energy_modifier = 10.f,
frezon_cool_rate_modifier = 20.f,
frezon_nitrogen_cool_ratio = 5.f,
frezon_cool_energy_released = -600000.f * heat_scale,

nitrium_decomp_temp = T0C + 70.f,
nitrium_decomposition_energy = 30000.f,

tickrate = 0.5f,

round_temp_to = 0.01f, round_pressure_to = 0.1f;

inline const size_t round_temp_dig = 2, round_pressure_dig = 1;

inline std::chrono::high_resolution_clock main_clock;

}

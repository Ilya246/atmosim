#pragma once

#include <chrono>

inline const float heat_scale = 8.0,

pressure_cap = 1013.25,
pipe_pressure_cap = 4500.0,
required_transfer_volume = 1400.0,
TCMB = 2.7,
T0C = 273.15,
T20C = T0C + 20.f,
fire_temp = T0C + 100.f,

minimum_heat_capacity = 0.0003,
R = 8.314462618,

one_atmosphere = 101.325,
tank_leak_pressure = 30.0 * one_atmosphere,
tank_rupture_pressure = 40.0 * one_atmosphere,
tank_fragment_pressure = 50.0 * one_atmosphere,
tank_fragment_scale = 2.0 * one_atmosphere,

fire_hydrogen_energy_released = 284000.0 * heat_scale,
minimum_tritium_oxyburn_energy = 143000.0,
tritium_burn_oxy_factor = 100.0,
tritium_burn_trit_factor = 10.0,

fire_plasma_energy_released = 160000.0 * heat_scale,
super_saturation_threshold = 96.0,
super_saturation_ends = super_saturation_threshold / 3.0,
oxygen_burn_rate_base = 1.4,
plasma_upper_temperature = 1643.15,
plasma_oxygen_fullburn = 10.0,
plasma_burn_rate_delta = 9.0,

n2o_decomp_temp = 850.0,
N2Odecomposition_rate = 0.5,

frezon_cool_temp = 23.15,
frezon_cool_lower_temperature = 23.15,
frezon_cool_mid_temperature = 373.15,
frezon_cool_maximum_energy_modifier = 10.0,
frezon_cool_rate_modifier = 20.0,
frezon_nitrogen_cool_ratio = 5.0,
frezon_cool_energy_released = -600000.0 * heat_scale,

nitrium_decomp_temp = T0C + 70.0,
nitrium_decomposition_energy = 30000.0,

tickrate = 0.5;

inline std::chrono::high_resolution_clock main_clock;

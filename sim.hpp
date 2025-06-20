#pragma once
#include <vector>
#include "gas.hpp"
#include "tank.hpp"

// Reaction logic
void do_plasma_fire();
void do_trit_fire();
void doN2ODecomposition();
void do_frezon_coolant();
void do_nitrium_decomposition();
void react();

// Simulation loop
void loop(int n);
void loop();
void loop_print();

// Input setup functions
void full_input_setup();
float get_gasmix_spec_heat(const std::vector<gas_type>& gases, const std::vector<float>& ratios);
float mix_input_setup(const std::vector<gas_type>& mix_gases, const std::vector<float>& mix_ratios, const std::vector<gas_type>& primer_gases, const std::vector<float>& primer_ratios, float fuel_temp, float primer_temp, float target_temp);
void known_input_setup(const std::vector<gas_type>& mix_gases, const std::vector<float>& mix_ratios, const std::vector<gas_type>& primer_gases, const std::vector<float>& primer_ratios, float fuel_temp, float primer_temp, float fuel_pressure);
float unimix_input_setup(gas_type gas1, gas_type gas2, float temp1, float temp2, float target_temp);
void unimix_to_input_setup(gas_type gas1, gas_type gas2, float temp, float second_per_first);
void heat_cap_input_setup(); 
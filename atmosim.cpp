#ifdef PLOT
#include <sciplot/sciplot.hpp>
#endif

#include "argparse/args.hpp"

#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

enum value_type {int_val, float_val, bool_val, none_val};

struct dyn_val {
    value_type type;
    void* value_ptr;

    bool invalid() {
        return type == none_val || value_ptr == nullptr;
    }
};

template <typename T>
T* get_dyn_ptr(dyn_val val) {
    return (T*)val.value_ptr;
}
template <typename T>
T& get_dyn(dyn_val val) {
    return *get_dyn_ptr<T>(val);
}

// generic system for specifying what you don't want atmosim to give you
struct base_restriction {
    virtual bool OK() = 0;
};

template <typename T>
struct num_restriction : base_restriction {
    T* value_ptr;
    T min_value;
    T max_value;

    num_restriction(T* ptr, T min, T max): value_ptr(ptr), min_value(min), max_value(max) {
        if (max_value < 0) {
            max_value = numeric_limits<T>::max();
        }
    }

    bool OK() override {
        return *value_ptr >= min_value
        &&     *value_ptr <= max_value;
    }
};

struct bool_restriction : base_restriction {
    bool* value_ptr;
    bool target_value;

    bool_restriction(bool* ptr, bool target): value_ptr(ptr), target_value(target) {}

    bool OK() override {
        return *value_ptr == target_value;
    }
};

float heat_scale = 1.0;


const int gas_count = 8;
float gas_amounts[gas_count]{};
float gas_heat_caps[gas_count]{20.f * heat_scale, 30.f * heat_scale, 200.f * heat_scale, 10.f * heat_scale, 40.f * heat_scale, 30.f * heat_scale, 600.f * heat_scale, 40.f * heat_scale};
const string gas_names[gas_count]{  "oxygen",         "nitrogen",       "plasma",          "tritium",        "water_vapour",    "carbon_dioxide",  "frezon",          "nitrous_oxide"  };

const int invalid_gas_num = -1;

// integer container struct denoting a gas type
struct gas_type {
    int gas = invalid_gas_num;

    float& amount() const {
        return gas_amounts[gas];
    }

    void update_amount(const float& delta, float& heat_capacity_cache) {
        amount() += delta;
        heat_capacity_cache += delta * heat_cap();
    }

    float& heat_cap() const {
        return gas_heat_caps[gas];
    }

    bool invalid() const {
        return gas == invalid_gas_num;
    }

    string name() const {
        return gas_names[gas];
    }

    bool operator== (const gas_type& other) {
        return gas == other.gas;
    }

    bool operator!= (const gas_type& other) {
        return gas != other.gas;
    }
};

gas_type oxygen{0};
gas_type nitrogen{1};
gas_type plasma{2};
gas_type tritium{3};
gas_type water_vapour{4};
gas_type carbon_dioxide{5};
gas_type frezon{6};
gas_type nitrous_oxide{7};
gas_type invalid_gas{invalid_gas_num};

gas_type gases[]{oxygen, nitrogen, plasma, tritium, water_vapour, carbon_dioxide, frezon, nitrous_oxide};

unordered_map<string, gas_type> gas_map{
    {"oxygen",        oxygen       },
    {"nitrogen",      nitrogen     },
    {"plasma",        plasma       },
    {"tritium",       tritium      },
    {"water_vapour",   water_vapour  },
    {"carbon_dioxide", carbon_dioxide},
    {"frezon",        frezon       },
    {"nitrous_oxide",  nitrous_oxide }};

string list_gases() {
    string out;
    for (gas_type g : gases) {
        out += g.name() + ", ";
    }
    out.resize(out.size() - 2);
    return out;
}

enum tank_state {
    intact = 0,
    ruptured = 1,
    exploded = 2
};

float temperature = 293.15, volume = 5.0, pressure_cap = 1013.25, pipe_pressure_cap = 4500.0, required_transfer_volume = 1400.0,
radius = 0.0,
leaked_heat = 0.0;
tank_state cur_state = intact;
int integrity = 3, leak_count = 0, tick = 0,
tick_cap = 30, pipe_tick_cap = 1000,
log_level = 1;
bool step_target_temp = false,
check_status = true,
simple_output = false, silent = false,
optimise_int = false, optimise_maximise = true, optimise_before = false;
float fire_temp = 373.15, minimum_heat_capacity = 0.0003, one_atmosphere = 101.325, R = 8.314462618,
tank_leak_pressure = 30.0 * one_atmosphere, tank_rupture_pressure = 40.0 * one_atmosphere, tank_fragment_pressure = 50.0 * one_atmosphere, tank_fragment_scale = 2.0 * one_atmosphere,
fire_hydrogen_energy_released = 284000.0 * heat_scale, minimum_tritium_oxyburn_energy = 143000.0, tritium_burn_oxy_factor = 100.0, tritium_burn_trit_factor = 10.0,
fire_plasma_energy_released = 160000.0 * heat_scale, super_saturation_threshold = 96.0, super_saturation_ends = super_saturation_threshold / 3.0, oxygen_burn_rate_base = 1.4, plasma_upper_temperature = 1643.15, plasma_oxygen_fullburn = 10.0, plasma_burn_rate_delta = 9.0,
n2o_decomp_temp = 850.0, N2Odecomposition_rate = 0.5,
frezon_cool_temp = 23.15, frezon_cool_lower_temperature = 23.15, frezon_cool_mid_temperature = 373.15, frezon_cool_maximum_energy_modifier = 10.0, frezon_cool_rate_modifier = 20.0, frezon_nitrogen_cool_ratio = 5.0, frezon_cool_energy_released = -600000.0 * heat_scale,
tickrate = 0.5,
over_temp = 0.1, temperature_step = 1.002, temperature_step_min = 0.1, ratio_step = 1.005, ratio_from = 10.0, ratio_to = 10.0, amplif_scale = 1.2, amplif_downscale = 1.4, max_amplif = 20.0, max_deriv = 1.005,
heat_capacity_cache = 0.0;
vector<gas_type> active_gases;
string rotator = "|/-\\";
int rotator_chars = 4;
int rotator_index = rotator_chars - 1;
long long progress_bar_spacing = 4817;
// ETA values are in ms
const long long progress_update_spacing = progress_bar_spacing * 25;
const int progress_polls = 20;
const long long progress_poll_window = progress_update_spacing * progress_polls;
long long progress_poll_times[progress_polls];
long long progress_poll = 0;
long long last_speed = 0;
chrono::high_resolution_clock main_clock;

dyn_val optimise_val = {float_val, &radius};
vector<base_restriction*> pre_restrictions;
vector<base_restriction*> post_restrictions;

bool restrictions_met(const vector<base_restriction*>& restrictions) {
    for (base_restriction* r : restrictions) {
        if (!r->OK()) {
            return false;
        }
    }
    return true;
}

char get_rotator() {
    rotator_index = (rotator_index + 1) % rotator_chars;
    return rotator[rotator_index];
}

unordered_map<string, dyn_val> sim_params{
    {"",            {none_val,  nullptr     }},
    {"radius",      {float_val, &radius     }},
    {"temperature", {float_val, &temperature}},
    {"leaked_heat",  {float_val, &leaked_heat }},
    {"ticks",       {int_val,   &tick       }},
    {"tank_state",   {int_val,   &cur_state  }}};

// ran at the start of main()
void setup_params() {
    for (gas_type g : gases) {
        sim_params["gases." + g.name()] = {float_val, &g.amount()};
    }
}

string list_params() {
    string out;
    for (const auto& [key, value] : sim_params) {
        out += key + ", ";
    }
    out.resize(out.size() - 2);
    return out;
}

dyn_val get_param(const string& name) {
    if (sim_params.contains(name)) {
        return sim_params[name];
    }
    return sim_params[""];
}

istream& operator>>(istream& stream, dyn_val& param) {
    string val;
    stream >> val;
    param = get_param(val);
    if (param.invalid()) {
        cin.setstate(ios_base::failbit);
    }
    return stream;
}

// flushes a basic_istream<char> until after \n
basic_istream<char>& flush_stream(basic_istream<char>& stream) {
    stream.ignore(numeric_limits<streamsize>::max(), '\n');
    return stream;
}

// query user input from keyboard, ask again if invalid
template <typename T>
T get_input(const string& what, const string& invalid_err = "Invalid value. Try again.\n") {
    bool valid = false;
    T val;
    while (!valid) {
        valid = true;
        cout << what;
        cin >> val;
        if (cin.fail() || cin.peek() != '\n') {
            cerr << invalid_err;
            cin.clear();
            flush_stream(cin);
            valid = false;
        }
    }
    return val;
}

// returns true if user entered nothing, false otherwise
bool await_input() {
    return flush_stream(cin).peek() == '\n';
}

// evaluates a string as an [y/n] option
bool eval_opt(const string& opt, bool default_opt = true) {
    return opt == "y" || opt == "Y" // is it Y?
    ||    (opt != "n" && opt != "N" && default_opt); // it's not Y, so check if it's not N, and if so, return default
}

// requests an [y/n] input from user
bool get_opt(const string& what, bool default_opt = true) {
    cout << what << (default_opt ? " [Y/n] " : " [y/N] ");

    if (await_input()) return default_opt; // did the user just press enter?

    string opt; // we have non-empty input so check what it is
    cin >> opt;
    return eval_opt(opt, default_opt);
}

void reset() {
    for (gas_type g : gases) {
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

bool is_gas(const string& gas) {
    return gas_map.contains(gas);
}

// string-to-gas
gas_type to_gas(const string& gas) {
    if (!is_gas(gas)) {
        return invalid_gas;
    }
    return gas_map[gas];
}

// string-to-gas but throw an exception if invalid
gas_type parse_gas(const string& gas) {
    gas_type out = to_gas(gas);
    if (out.invalid()) {
        throw invalid_argument("Parsed invalid gas type.");
    }
    return out;
}

istream& operator>>(istream& stream, gas_type& g) {
    string val;
    stream >> val;
    g = to_gas(val);
    if (g == invalid_gas) {
        cin.setstate(ios_base::failbit);
    }
    return stream;
}

float get_heat_capacity() {
    float sum = 0.0;
    for (const gas_type& g : gases) {
        sum += g.amount() * g.heat_cap();
    }
    return sum;
}
void update_heat_capacity(const gas_type& type, const float& moles_delta, float& capacity) {
    capacity += type.heat_cap() * moles_delta;
}
float get_gas_mols() {
    float sum = 0.0;
    for (const gas_type& g : gases) {
        sum += g.amount();
    }
    return sum;
}
float pressure_temp_to_mols(float pressure, float temp) {
    return pressure * volume / temp / R;
}
float mols_temp_to_pressure(float mols, float temp) {
    return mols * R * temp / volume;
}
float gases_temps_to_temp(gas_type gas1, float temp1, gas_type gas2, float temp2) {
    return (gas1.amount() * temp1 * gas1.heat_cap() + gas2.amount() * temp2 * gas2.heat_cap()) / (gas1.amount() * gas1.heat_cap() + gas2.amount() * gas2.heat_cap());
}
float mix_gas_temps_to_temp(float gasc1, float gashc1, float temp1, gas_type gas2, float temp2) {
    return (gasc1 * temp1 * gashc1 + gas2.amount() * temp2 * gas2.heat_cap()) / (gasc1 * gashc1 + gas2.amount() * gas2.heat_cap());
}
float get_pressure() {
    return get_gas_mols() * R * temperature / volume;
}
float get_cur_range() {
    return sqrt((get_pressure() - tank_fragment_pressure) / tank_fragment_scale);
}

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

void react() {
    heat_capacity_cache = get_heat_capacity();
    if (temperature >= frezon_cool_temp && nitrogen.amount() >= 0.01f && frezon.amount() >= 0.01f) {
        do_frezon_coolant();
    }
    if (temperature >= n2o_decomp_temp && nitrous_oxide.amount() >= 0.01f) {
        doN2ODecomposition();
    }
    if (temperature >= fire_temp && oxygen.amount() >= 0.01f) {
        if (tritium.amount() >= 0.01f) {
            do_trit_fire();
        }
        if (plasma.amount() >= 0.01f) {
            do_plasma_fire();
        }
    }
}
void tank_check_status() {
    float pressure = get_pressure();
    if (pressure > tank_leak_pressure) {
        if (pressure > tank_rupture_pressure) {
            if (pressure > tank_fragment_pressure) {
                for (int i = 0; i < 3; ++i) {
                    react();
                }
                cur_state = exploded;
                radius = get_cur_range();
                for (gas_type g : gases) {
                    leaked_heat += g.amount() * g.heat_cap() * temperature;
                }
                return;
            }
            if (integrity <= 0) {
                cur_state = ruptured;
                radius = 0.0;
                for (gas_type g : gases) {
                    leaked_heat += g.amount() * g.heat_cap() * temperature;
                }
                return;
            }
            integrity--;
            return;
        }
        if (integrity <= 0) {
            for (gas_type g : gases) {
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
    cout << "TICK: " << tick << " || Status: pressure " << get_pressure() << "k_pa \\ integrity " << integrity << " \\ temperature " << temperature << "K\n_contents: ";
    for (gas_type g : gases) {
        cout << g.name() << ": " << g.amount() << " mol; ";
    }
    cout << endl;
    if (cur_state == exploded) {
        cout << "EXPLOSION: range " << get_cur_range() << endl;
    } else if (cur_state == ruptured) {
        cout << "RUPTURED" << endl;
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
float mix_input_setup(gas_type gas1, gas_type gas2, gas_type into, float fuel_temp, float into_temp, float target_temp, float second_per_first) {
    float specheat = (gas1.heat_cap() + gas2.heat_cap() * second_per_first) / (1.0 + second_per_first);
    float fuel_pressure = (target_temp / into_temp - 1.0) * pressure_cap / (specheat / into.heat_cap() - 1.0 + target_temp * (1.0 / into_temp - specheat / into.heat_cap() / fuel_temp));
    float fuel = pressure_temp_to_mols(fuel_pressure, fuel_temp);
    gas1.amount() = fuel / (1.0 + second_per_first);
    gas2.amount() = fuel - gas1.amount();
    into.amount() = pressure_temp_to_mols(pressure_cap - fuel_pressure, into_temp);
    temperature = mix_gas_temps_to_temp(fuel, specheat, fuel_temp, into, into_temp);
    return fuel_pressure;
}
void known_input_setup(gas_type gas1, gas_type gas2, gas_type into, float fuel_temp, float into_temp, float fuel_pressure, float second_per_first) {
    float specheat = (gas1.heat_cap() + gas2.heat_cap() * second_per_first) / (1.0 + second_per_first);
    float fuel = pressure_temp_to_mols(fuel_pressure, fuel_temp);
    gas1.amount() = fuel / (1.0 + second_per_first);
    gas2.amount() = fuel - gas1.amount();
    into.amount() = pressure_temp_to_mols(pressure_cap - fuel_pressure, into_temp);
    temperature = mix_gas_temps_to_temp(fuel, specheat, fuel_temp, into, into_temp);
}
float unimix_input_setup(gas_type gas1, gas_type gas2, float temp1, float temp2, float target_temp) {
    float fuel_pressure = (target_temp / temp2 - 1.0) * pressure_cap / (gas1.heat_cap() / gas2.heat_cap() - 1.0 + target_temp * (1.0 / temp2 - gas1.heat_cap() / gas2.heat_cap() / temp1));
    gas1.amount() = pressure_temp_to_mols(fuel_pressure, temp1);
    gas2.amount() = pressure_temp_to_mols(pressure_cap - fuel_pressure, temp2);
    temperature = mix_gas_temps_to_temp(gas1.amount(), gas1.heat_cap(), temp1, gas2, temp2);
    return fuel_pressure;
}
void unimix_to_input_setup(gas_type gas1, gas_type gas2, float temp, float second_per_first) {
    temperature = temp;
    float total = pressure_temp_to_mols(pressure_cap, temperature);
    gas1.amount() = total / (1.0 + second_per_first);
    gas2.amount() = total - gas1.amount();
}

struct bomb_data {
    float ratio, fuel_temp, fuel_pressure, thir_temp, mix_pressure, mix_temp;
    gas_type gas1, gas2, gas3;
    tank_state state = intact;
    float radius = 0.0, fin_temp = -1.0, fin_pressure = -1.0, fin_heat_leak = -1.0, optstat = -1.0;
    int ticks = -1;

    bomb_data(float ratio, float fuel_temp, float fuel_pressure, float thir_temp, float mix_pressure, float mix_temp, gas_type gas1, gas_type gas2, gas_type gas3):
        ratio(ratio), fuel_temp(fuel_temp), fuel_pressure(fuel_pressure), thir_temp(thir_temp), mix_pressure(mix_pressure), mix_temp(mix_temp), gas1(gas1), gas2(gas2), gas3(gas3) {};

    void results(float n_radius, float n_fin_temp, float n_fin_pressure, float n_optstat, int n_ticks, tank_state n_state) {
        radius = n_radius;
        fin_temp = n_fin_temp;
        fin_pressure = n_fin_pressure;
        optstat = n_optstat;
        ticks = n_ticks;
        state = n_state;
    }

    string print_very_simple() const {
        float first_fraction = 1.f / (1.f + ratio);
        return string(to_string(fuel_temp) + " " + to_string(fuel_pressure) + " " + to_string(first_fraction) + " " + to_string(thir_temp));
    }

    string print_simple() const {
        float first_fraction = 1.f / (1.f + ratio);
        float second_fraction = ratio * first_fraction;
        return string(
        "TANK: { " ) +
            "mix: [ " +
                to_string(100.f * first_fraction) + "%:" + to_string(100.f * second_fraction) + "% | " +
                "temp " + to_string(fuel_temp) + "K | " +
                "pressure " + to_string(fuel_pressure) + "k_pa " +
            "]; " +
            "third: [ " +
                "temp " + to_string(thir_temp) + "K " +
            "]; " +
            "end state: [ " +
                "ticks " + to_string(ticks) + "t | " + (
                state == exploded ?
                "radius " + to_string(radius) + "til "
                : state == ruptured ? "ruptured " : "no explosion " ) +
            "] " +
            "optstat: " + to_string(optstat) + " " +
        "}";
    }

    string print_extensive() const {
        float first_fraction = 1.f / (1.f + ratio);
        float second_fraction = ratio * first_fraction;
        float volume_ratio = (required_transfer_volume + volume) / volume;
        float added_ratio = (required_transfer_volume + volume) / required_transfer_volume;
        return string(
        "TANK: {\n" ) +
            "\tinitial state: [\n" +
                "\t\ttemperature\t" + to_string(mix_temp) + " K\n" +
                "\t\tpressure\t" + to_string(mix_pressure) + " k_pa\n" +
                "\t\t" + gas1.name() + to_string(pressure_temp_to_mols(first_fraction * fuel_pressure, fuel_temp)) + " mol\t" + "\n" +
                "\t\t" + gas2.name() + to_string(pressure_temp_to_mols(second_fraction * fuel_pressure, fuel_temp)) + " mol\t" + "\n" +
                "\t\t" + gas3.name() + to_string(pressure_temp_to_mols(pressure_cap - fuel_pressure, thir_temp)) + " mol\t" + "\n" +
            "\t];\n" +
            "\tend state: [\n" +
                "\t\ttime\t\t" + to_string(ticks * tickrate) + " s\n" +
                "\t\tpressure \t" + to_string(fin_pressure) + " k_pa\n" +
                "\t\ttemperature\t" + to_string(fin_temp) + " K\n" +
                "\t\t" + (
                state == exploded ?
                "explosion\t" + to_string(radius) + " tiles "
                : state == ruptured ? "ruptured" : "no explosion" ) + "\n" +
            "\t]\n" +
            "\toptstat\t" + to_string(optstat) + "\n" +
        "};\n" +
        "REQUIREMENTS: {\n" +
            "\tmix-canister (fuel): [\n" +
                "\t\tgas ratio\t" + gas1.name() + "\t\t" + gas2.name() + "\n" +
                "\t\tgas ratio\t" + to_string(100.f * first_fraction) + "%\t" + to_string(100.f * second_fraction) + "%\n" +
                "\t\tgas ratio\t" + to_string(1.0/ratio) + "\n" +
                "\t\ttemperature\t" + to_string(fuel_temp) + " K\n" +
                "\t\ttank pressure\t" + to_string(fuel_pressure) + " k_pa\n" +
                "\t\tleast-mols: [\n" +
                    "\t\t\t" + gas1.name() + "\t\t" + gas2.name() + "\n" +
                    "\t\t\t" + to_string(pressure_temp_to_mols(first_fraction * fuel_pressure, fuel_temp) * volume_ratio) + "\t" + to_string(pressure_temp_to_mols(second_fraction * fuel_pressure, fuel_temp) * volume_ratio) + "\n" +
                "\t\t]\n"
            "\t];\n" +
            "\tthird-canister (primer): [\n" +
                "\t\ttemperature\t" + to_string(thir_temp) + " K\n" +
                "\t\tpressure\t" + to_string((pressure_cap * 2.0 - fuel_pressure) * added_ratio) + " k_pa\n" +
                "\t\tleast-mols:\t" + to_string(pressure_temp_to_mols(pressure_cap * 2.0 - fuel_pressure, thir_temp) * volume_ratio) + " mol\t" + gas3.name() + "\n" +
            "\t]\n" +
        "}\n" +
        "REVERSE-REQUIREMENTS: {\n" +
            "\tmix-canister (primer): [\n" +
                "\t\tgas ratio\t" + gas1.name() + "\t\t" + gas2.name() + "\n" +
                "\t\tgas ratio\t" + to_string(100.f * first_fraction) + "%\t" + to_string(100.f * second_fraction) + "%\n" +
                "\t\tgas ratio\t" + to_string(1.0/ratio) + "\n" +
                "\t\ttemperature\t" + to_string(fuel_temp) + " K\n" +
                "\t\tpressure\t" + to_string((pressure_cap + fuel_pressure) * added_ratio) + " k_pa\n" +
                "\t\tleast-mols: [\n" +
                    "\t\t\t" + gas1.name() + "\t\t" + gas2.name() + "\n" +
                    "\t\t\t" + to_string(pressure_temp_to_mols(pressure_cap + fuel_pressure, fuel_temp) * first_fraction * volume_ratio) + "\t" + to_string(pressure_temp_to_mols(pressure_cap + fuel_pressure, fuel_temp) * second_fraction * volume_ratio) + "\n" +
                "\t\t];\n" +
            "\t];\n" +
            "\tthird-canister (fuel): [\n" +
                "\t\ttemperature\t" + to_string(thir_temp) + " K\n" +
                "\t\ttank pressure\t" + to_string((pressure_cap - fuel_pressure)) + " k_pa\n" +
                "\t\tleast-mols:\t" + to_string(pressure_temp_to_mols(pressure_cap - fuel_pressure, thir_temp) * volume_ratio) + " mol\t" + gas3.name() + "\n" +
            "\t]\n" +
        "}";
    }
};

void print_bomb(const bomb_data& bomb, const string& what, bool extensive = false) {
    cout << what << (simple_output ? bomb.print_very_simple() : (extensive ? bomb.print_extensive() : bomb.print_simple())) << endl;
}
string get_progress_bar(long progress, long size) {
    string progress_bar = '[' + string(progress, '#') + string(size - progress, ' ') + ']';
    return progress_bar;
}
void print_progress(long long iters, auto start_time) {
    printf("%lli Iterations %c ", iters, get_rotator());
    if (iters % progress_update_spacing == 0) {
        long long cur_time = chrono::duration_cast<chrono::milliseconds>(main_clock.now() - start_time).count();
        progress_poll_times[progress_poll] = cur_time;
        progress_poll = (progress_poll + 1) % progress_polls;
        long long poll_time = progress_poll_times[progress_poll];
        long long time_passed = cur_time - poll_time;
        float progress_passed = std::min(progress_poll_window, iters);
        last_speed = (float)progress_passed / time_passed * 1000.f;
    }
    printf("[Speed: %lli iters/s]\r", last_speed);
    cout.flush();
}


#ifdef PLOT
void plot_current(float stats[], vector<float> x_vals[], vector<float> y_vals[], float cur_value, const int i) {
    float stat = stats[i];
    x_vals[i].push_back(cur_value);
    y_vals[i].push_back(stat);
}
void check_reset_plot(vector<float> x_vals[], vector<float> y_vals[], vector<float> tempXVals[], vector<float> tempYVals[], float global_best_stats[], float last_best_stats[], const int i) {
    if (global_best_stats[i] != last_best_stats[i]) {
        x_vals[i] = tempXVals[i];
        y_vals[i] = tempYVals[i];
        last_best_stats[i] = global_best_stats[i];
    }
    tempXVals[i].clear();
    tempYVals[i].clear();
}
#endif

float optimise_stat() {
    return optimise_val.type == float_val ? get_dyn<float>(optimise_val) : get_dyn<int>(optimise_val);
}
void update_amplif(float last_stats[], float amplifs[], float stats[], const int i, bool maximise) {
    float stat = stats[i];
    float deriv = stat / last_stats[i];
    float abs_deriv = maximise ? deriv : 1.f / deriv;
    float& amplif = amplifs[i];
    amplif = std::max(1.f, amplif * (abs_deriv > max_deriv && abs_deriv == abs_deriv ? 1.f / (abs_deriv / max_deriv) / amplif_downscale : amplif_scale));
    amplif = std::min(amplif, max_amplif);
    last_stats[i] = stat;
    stats[i] = maximise ? numeric_limits<float>::min() : numeric_limits<float>::max();
}
bomb_data test_twomix(gas_type gas1, gas_type gas2, gas_type gas3, float mixt1, float mixt2, float thirt1, float thirt2, bool maximise, bool measure_before) {
    // parameters of the tank with the best result we have so far
    bomb_data best_bomb(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, gas1, gas2, gas3);
    best_bomb.optstat = maximise ? numeric_limits<float>::min() : numeric_limits<float>::max();

    // same but only best in the current surrounding frame
    bomb_data best_bomb_local(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, gas1, gas2, gas3);
    best_bomb_local.optstat = maximise ? numeric_limits<float>::min() : numeric_limits<float>::max();

    #ifdef PLOT
    sciplot::Plot2D plot1, plot2, plot3, plot4;
    vector<float> x_vals[4];
    vector<float> y_vals[4];
    vector<float> x_vals_temp[4];
    vector<float> y_vals_temp[4];
    float global_best_stats[4] {1.f, 1.f, 1.f, 1.f};
    float last_best_stats[4] {1.f, 1.f, 1.f, 1.f};
    #endif

    long long iters = 0;
    chrono::time_point start_time = main_clock.now();
    float last_stats[4] {1.f, 1.f, 1.f, 1.f};
    float amplifs[4] {1.f, 1.f, 1.f, 1.f};
    float best_stats[4] {1.f, 1.f, 1.f, 1.f};
    for (float thir_temp = thirt1; thir_temp <= thirt2; thir_temp = std::max(thir_temp * (1.f + (temperature_step - 1.f) * amplifs[0]), thir_temp + temperature_step_min * amplifs[0])) {
        for (float fuel_temp = mixt1; fuel_temp <= mixt2; fuel_temp = std::max(fuel_temp * (1.f + (temperature_step - 1.f) * amplifs[1]), fuel_temp + temperature_step_min * amplifs[1])) {
            float target_temp2 = step_target_temp ? std::max(thir_temp, fuel_temp) : fire_temp + over_temp + temperature_step;
            for (float target_temp = fire_temp + over_temp; target_temp < target_temp2; target_temp = std::max(target_temp * (1.f + (temperature_step - 1.f) * amplifs[2]), target_temp + temperature_step_min * amplifs[2])) {
                for (float ratio = 1.0 / ratio_from; ratio <= ratio_to; ratio += ratio * (ratio_step - 1.f) * amplifs[3]) {
                    ++iters;
                    if (iters % progress_bar_spacing == 0) {
                        print_progress(iters, start_time);
                    }
                    float fuel_pressure, stat;
                    reset();
                    if (fuel_temp <= fire_temp && thir_temp <= fire_temp) {
                        continue;
                    }
                    if ((target_temp > fuel_temp) == (target_temp > thir_temp)) {
                        continue;
                    }
                    fuel_pressure = mix_input_setup(gas1, gas2, gas3, fuel_temp, thir_temp, target_temp, ratio);
                    if (fuel_pressure > pressure_cap || fuel_pressure < 0.0) {
                        continue;
                    }
                    if (!restrictions_met(pre_restrictions)) {
                        continue;
                    }
                    if (measure_before) {
                        stat = optimise_stat();
                    }
                    float mix_pressure = get_pressure();
                    loop();
                    if (!measure_before) {
                        stat = optimise_stat();
                    }
                    bool no_discard = restrictions_met(post_restrictions);
                    bomb_data cur_bomb(ratio, fuel_temp, fuel_pressure, thir_temp, mix_pressure, target_temp, gas1, gas2, gas3);
                    cur_bomb.results(radius, temperature, get_pressure(), stat, tick, cur_state);
                    if (no_discard && (maximise == (stat > best_bomb.optstat))) {
                        best_bomb = cur_bomb;
                    }
                    if (log_level >= 5) {
                        print_bomb(cur_bomb, "\n", true);
                    }
                    if (no_discard && (maximise == (stat > best_bomb_local.optstat))) {
                        best_bomb_local = cur_bomb;
                    }
                    for (float& s : best_stats) {
                        if (no_discard && (maximise == (stat > s))) {
                            s = stat;
                        }
                    }
                    #ifdef PLOT
                    for (float& s : global_best_stats) {
                        if (no_discard && (maximise == (stat > s))) {
                            s = stat;
                        }
                    }
                    plot_current(best_stats, x_vals_temp, y_vals_temp, ratio, 3);
                    #endif
                    update_amplif(last_stats, amplifs, best_stats, 3, maximise);
                }
                #ifdef PLOT
                check_reset_plot(x_vals, y_vals, x_vals_temp, y_vals_temp, global_best_stats, last_best_stats, 3);
                plot_current(best_stats, x_vals_temp, y_vals_temp, target_temp, 2);
                #endif
                update_amplif(last_stats, amplifs, best_stats, 2, maximise);
                if (log_level == 4) {
                    print_bomb(best_bomb_local, "Current: ");
                    best_bomb_local.optstat = maximise ? numeric_limits<float>::min() : numeric_limits<float>::max();
                }
            }
            #ifdef PLOT
            check_reset_plot(x_vals, y_vals, x_vals_temp, y_vals_temp, global_best_stats, last_best_stats, 2);
            plot_current(best_stats, x_vals_temp, y_vals_temp, fuel_temp, 1);
            #endif
            update_amplif(last_stats, amplifs, best_stats, 1, maximise);
            if (log_level == 3) {
                print_bomb(best_bomb_local, "Current: ");
                best_bomb_local.optstat = maximise ? numeric_limits<float>::min() : numeric_limits<float>::max();
            }
        }
        #ifdef PLOT
        check_reset_plot(x_vals, y_vals, x_vals_temp, y_vals_temp, global_best_stats, last_best_stats, 1);
        plot_current(best_stats, x_vals, y_vals, thir_temp, 0);
        #endif
        update_amplif(last_stats, amplifs, best_stats, 0, maximise);
        if (log_level == 2) {
            print_bomb(best_bomb_local, "Current: ");
            best_bomb_local.optstat = maximise ? numeric_limits<float>::min() : numeric_limits<float>::max();
        } else if (log_level == 1) {
            print_bomb(best_bomb_local, "Best: ");
        }
    }
    #ifdef PLOT
    plot4.draw_curve(x_vals[3], y_vals[3]).label("ratio->optstat");
    plot4.xtics().logscale(2);
    plot3.draw_curve(x_vals[2], y_vals[2]).label("target_temp->optstat");
    plot2.draw_curve(x_vals[1], y_vals[1]).label("fuel_temp->optstat");
    plot1.draw_curve(x_vals[0], y_vals[0]).label("thir_temp->optstat");
    sciplot::Figure fig = {{plot1, plot2}, {plot3, plot4}};
    sciplot::Canvas canv = {{fig}};
    canv.size(900, 900);
    canv.show();
    #endif
    return best_bomb;
}

void heat_cap_input_setup() {
    cout << "Enter heat capacities for " << list_gases() << ": ";
    for (gas_type g : gases) {
        cin >> g.heat_cap();
    };
}

int main(int argc, char* argv[]) {
    // setup
    setup_params();

    gas_type gas1, gas2, gas3;
    float mixt1 = 0.0, mixt2 = 0.0, thirt1 = 0.0, thirt2 = 0.0;

    bool redefine_heatcap = false, set_ratio_iter = false, mixing_mode = false, manual_mix = false, do_retest = false, ask_param = false, ask_restrict = false;

    std::vector<std::shared_ptr<base_argument>> args = {
        make_argument("pipeonly", "", "assume inside pipe: prevent tank-related effects", check_status),
        make_argument("redefineheatcap", "", "redefine heat capacities", redefine_heatcap),
        make_argument("ratioiter", "", "set gas ratio iteration bounds+step", set_ratio_iter),
        make_argument("mixtoiter", "s", "provide potentially better results by also iterating the mix-to temperature (WARNING: will take many times longer to calculate)", step_target_temp),
        make_argument("mixingmode", "m", "different-temperature gas mixer ratio calculator", mixing_mode),
        make_argument("manualmix", "f", "try full input: lets you manually input and a tank's contents and see what it does", manual_mix),
        make_argument("gas1", "g1", "the type of the first gas in the mix gas (usually fuel, in tank)", gas1),
        make_argument("gas2", "g2", "the type of the second gas in the mix gas (usually fuel, in tank)", gas2),
        make_argument("gas3", "g3", "the type of the third gas (usually primer, goes into tank to detonate)", gas3),
        make_argument("mixt1", "m1", "temperatures for this and the following options are in kelvin", mixt1),
        make_argument("mixt2", "m2", "the maximum of the temperature range to check for the mix gas", mixt2),
        make_argument("thirt1", "t1", "the minimum of the temperature range to check for the third gas", thirt1),
        make_argument("thirt2", "t2", "the maximum of the temperature range to check for the third gas", thirt2),
        make_argument("doretest", "", "after calculating the bomb, test it again and print every tick as it reacts", do_retest),
        make_argument("ticks", "t", "set tick limit: aborts if a bomb takes longer than this to detonate (default: " + to_string(tick_cap) + ")", tick_cap),
        make_argument("tstep", "", "set temperature iteration multiplier (default " + to_string(temperature_step) + ")", temperature_step),
        make_argument("tstepm", "", "set minimum temperature iteration step (default " + to_string(temperature_step_min) + ")", temperature_step_min),
        make_argument("volume", "v", "set tank volume (default " + to_string(volume) + ")", volume),
        make_argument("overtemp", "o", "only consider bombs which mix to this much above the ignition temperature; higher values may make bombs more robust to slight mismixing (default " + to_string(over_temp) + ")", over_temp),
        make_argument("loglevel", "l", "what level of the nested loop to log, 0-6: none, [default] global_best, thir_temp, fuel_temp, target_temp, all, debug", log_level),
        make_argument("param", "p", "lets you configure what and how to optimise", ask_param),
        make_argument("restrict", "r", "lets you make atmosim not consider bombs outside of chosen parameters", ask_restrict),
        make_argument("simpleout", "", "makes very simple output, for use by other programs or advanced users", simple_output),
        make_argument("silent", "", "output ONLY the final result, overrides loglevel", silent),
        make_argument("amplifscale", "", "how aggressively to speed up over regions with worsening optval (default " + to_string(amplif_scale) + ")", amplif_scale),
        make_argument("maxamplif", "", "maximum speedup over regions with worsening optval (default " + to_string(max_amplif) + ")", max_amplif),
        make_argument("maxderiv", "", "target optval increase; can be lowered for less aggressive iteration and better results (default " + to_string(max_deriv) + ")", max_deriv)
    };

    parse_arguments(args, argc, argv);
    if (ask_param) {
        cout << "Possible optimisations: " << list_params() << endl;
        optimise_val = get_input<dyn_val>("Enter what to optimise: ");
        optimise_maximise = get_opt("Maximise?");
        optimise_before = get_opt("Measure stat before ignition?", false);
    }
    if (ask_restrict) {
        while (true) {
            string restrict_what = "";
            cout << "Available parameters: " << list_params() << endl;
            cout << "Enter -1 as the upper limit on numerical restrictions to have no limit." << endl;
            dyn_val opt_val = get_input<dyn_val>("Enter what to restrict: ");
            bool valid = false;
            base_restriction* restrict;
            switch (opt_val.type) {
                case (int_val): {
                    int minv = get_input<int>("Enter lower limit: ");
                    int maxv = get_input<int>("Enter upper limit: ");
                    restrict = new num_restriction<int>(get_dyn_ptr<int>(opt_val), minv, maxv);
                    valid = true;
                    break;
                }
                case (float_val): {
                    float minv = get_input<float>("Enter lower limit: ");
                    float maxv = get_input<float>("Enter upper limit: ");
                    restrict = new num_restriction<float>(get_dyn_ptr<float>(opt_val), minv, maxv);
                    valid = true;
                    break;
                }
                case (bool_val): {
                    restrict = new bool_restriction(get_dyn_ptr<bool>(opt_val), get_opt("Enter target value:"));
                    valid = true;
                    break;
                }
                default: {
                    cout << "Invalid parameter." << endl;
                    break;
                }
            }
            if (valid) {
                if (get_opt("Restrict (Y=after | N=before) simulation done?")) {
                    post_restrictions.push_back(restrict);
                } else {
                    pre_restrictions.push_back(restrict);
                }
            }
            if (!get_opt("Continue?", false)) {
                break;
            }
        }
    }
    if (redefine_heatcap) {
        heat_cap_input_setup();
    }
    if (set_ratio_iter) {
        ratio_from = get_input<float>("max gas1:gas2: ");
        ratio_to = get_input<float>("max gas2:gas1: ");
        ratio_step = get_input<float>("ratio step: ");
    }
    if (mixing_mode && manual_mix) {
        cerr << "2 modes enabled at the same time. Choose one. Exiting" << endl;
        return 1;
    }
    if (mixing_mode) {
        float t1, t2, ratio, capratio;
        t1 = get_input<float>("temp1: ");
        t2 = get_input<float>("temp2: ");
        ratio = get_input<float>("molar ratio (first%): ");
        capratio = get_input<float>("second:first heat capacity ratio (omit if end temperature does not matter): ");

        ratio = 100.0 / ratio - 1.0;
        cout << "pressure ratio: " << 100.0 / (1.0 + ratio * t2 / t1) << "% first | temp " << (t1 + t2 * capratio * ratio) / (1.0 + capratio * ratio) << "K";
        return 0;
    }
    if (manual_mix) {
        full_input_setup();
        loop_print();
        status();
        return 0;
    }
    if (silent) {
        // stop talking, be quiet for several days
        cout.setstate(ios::failbit);
    }
    // TODO: unhardcode parameter selection
    // didn't exit prior, test 1 gas -> 2-gas-mix tanks
    bool any_invalid = gas1.invalid() || gas2.invalid() || gas3.invalid();
    if (any_invalid && !silent) {
        cout << "Gases: " << list_gases() << endl;
    }
    if (gas1.invalid()) {
        gas1 = get_input<gas_type>("First gas of mix: ");
    }
    if (gas2.invalid()) {
        gas2 = get_input<gas_type>("Second gas of mix: ");
    }
    if (gas3.invalid()) {
        gas3 = get_input<gas_type>("Inserted gas: ");
    }

    if (!mixt1) {
        mixt1 = get_input<float>("mix temp min: ");
    }
    if (!mixt2) {
        mixt2 = get_input<float>("mix temp max: ");
    }
    if (!thirt1) {
        thirt1 = get_input<float>("inserted temp min: ");
    }
    if (!thirt2) {
        thirt2 = get_input<float>("inserted temp max: ");
    }

    bomb_data best_bomb = test_twomix(gas1, gas2, gas3, mixt1, mixt2, thirt1, thirt2, optimise_maximise, optimise_before);
    cout.clear();
    cout << (simple_output ? "" : "Best:\n") << (simple_output ? best_bomb.print_very_simple() : best_bomb.print_extensive()) << endl;
    if (silent) {
        cout.setstate(ios::failbit);
    }
    if (do_retest) {
        reset();
        known_input_setup(gas1, gas2, gas3, best_bomb.fuel_temp, best_bomb.thir_temp, best_bomb.fuel_pressure, best_bomb.ratio);
        loop_print();
    }
    return 0;
}

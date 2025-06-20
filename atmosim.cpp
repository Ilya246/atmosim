#include "argparse/args.hpp"
#include "argparse/read.hpp"

#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
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

template<>
string argp::type_sig<dyn_val> = "param";

istream& operator>>(istream&, dyn_val&);

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
    virtual ~base_restriction() {};
    virtual bool OK() = 0;
};

template<>
string argp::type_sig<shared_ptr<base_restriction>> = "restriction";

// make argp treat restriction as a container for [] syntax
template<>
struct argp::is_container<std::shared_ptr<base_restriction>> : std::true_type {};

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

istream& operator>>(istream& stream, shared_ptr<base_restriction>& restriction) {
    try {
        string all;
        stream >> all;
        string_view view(all);
        size_t cpos = view.find(',');
        string_view param_s = view.substr(1, cpos - 1);
        dyn_val param = argp::parse_value<dyn_val>(param_s);
        if (param.invalid()) {
            stream.setstate(ios_base::failbit);
            return stream;
        }
        switch (param.type) {
            case (int_val): {
                size_t cpos2 = view.find(',', cpos + 1);
                int restrA = argp::parse_value<int>(view.substr(cpos + 1, cpos2 - cpos - 1));
                int restrB = argp::parse_value<int>(view.substr(cpos2 + 1, view.size() - cpos2 - 2));
                restriction = make_shared<num_restriction<int>>(num_restriction<int>((int*)param.value_ptr, restrA, restrB));
                break;
            }
            case (float_val): {
                size_t cpos2 = view.find(',', cpos + 1);
                float restrA = argp::parse_value<float>(view.substr(cpos + 1, cpos2 - cpos - 1));
                float restrB = argp::parse_value<float>(view.substr(cpos2 + 1, view.size() - cpos2 - 2));
                restriction = make_shared<num_restriction<float>>((float*)param.value_ptr, restrA, restrB);
                break;
            }
            case (bool_val): {
                bool restr = argp::parse_value<bool>(view.substr(1, view.size() - 2));
                restriction = make_shared<bool_restriction>((bool*)param.value_ptr, restr);
                break;
            }
            default: {
                stream.setstate(ios_base::failbit);
                break;
            }
        }
    } catch (const argp::read_error& e) {
        stream.setstate(ios_base::failbit);
    }
    return stream;
}

uint32_t __xorshift_seed = rand();
uint32_t xorshift_rng() {
    uint32_t& x = __xorshift_seed;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}

float frand() {
    static random_device rd;
    static mt19937 gen(rd());
    static uniform_real_distribution<float> distribution;

    return distribution(gen);
}


float heat_scale = 1.0;

const int gas_count = 9;
float gas_amounts[gas_count]{};
float gas_heat_caps[gas_count]{20.f * heat_scale, 30.f * heat_scale, 200.f * heat_scale, 10.f * heat_scale, 40.f * heat_scale, 30.f * heat_scale, 600.f * heat_scale, 40.f * heat_scale, 10.f * heat_scale};
const string gas_names[gas_count]{  "oxygen",         "nitrogen",       "plasma",          "tritium",        "water_vapour",    "carbon_dioxide",  "frezon",          "nitrous_oxide",  "nitrium"};

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
gas_type nitrium{8};
gas_type invalid_gas{invalid_gas_num};

gas_type gases[]{oxygen, nitrogen, plasma, tritium, water_vapour, carbon_dioxide, frezon, nitrous_oxide, nitrium};

unordered_map<string, gas_type> gas_map{
    {"oxygen",         oxygen         },
    {"nitrogen",       nitrogen       },
    {"plasma",         plasma         },
    {"tritium",        tritium        },
    {"water_vapour",   water_vapour   },
    {"carbon_dioxide", carbon_dioxide },
    {"frezon",         frezon         },
    {"nitrous_oxide",  nitrous_oxide  },
    {"nitrium",        nitrium        }
};

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
tick_cap = 60, pipe_tick_cap = 1000,
log_level = 1;
bool step_target_temp = false,
check_status = true,
simple_output = false, silent = false,
optimise_int = false, optimise_maximise = true, optimise_before = false;
float TCMB = 2.7, T0C = 273.15, T20C = 293.15,
fire_temp = 373.15, minimum_heat_capacity = 0.0003, one_atmosphere = 101.325, R = 8.314462618,
tank_leak_pressure = 30.0 * one_atmosphere, tank_rupture_pressure = 40.0 * one_atmosphere, tank_fragment_pressure = 50.0 * one_atmosphere, tank_fragment_scale = 2.0 * one_atmosphere,
fire_hydrogen_energy_released = 284000.0 * heat_scale, minimum_tritium_oxyburn_energy = 143000.0, tritium_burn_oxy_factor = 100.0, tritium_burn_trit_factor = 10.0,
fire_plasma_energy_released = 160000.0 * heat_scale, super_saturation_threshold = 96.0, super_saturation_ends = super_saturation_threshold / 3.0, oxygen_burn_rate_base = 1.4, plasma_upper_temperature = 1643.15, plasma_oxygen_fullburn = 10.0, plasma_burn_rate_delta = 9.0,
n2o_decomp_temp = 850.0, N2Odecomposition_rate = 0.5,
frezon_cool_temp = 23.15, frezon_cool_lower_temperature = 23.15, frezon_cool_mid_temperature = 373.15, frezon_cool_maximum_energy_modifier = 10.0, frezon_cool_rate_modifier = 20.0, frezon_nitrogen_cool_ratio = 5.0, frezon_cool_energy_released = -600000.0 * heat_scale,
nitrium_decomp_temp = T0C + 70.0, nitrium_decomposition_energy = 30000.0,
tickrate = 0.5,
lower_target_temp = fire_temp + 0.1, temperature_step = 1.002, temperature_step_min = 0.05, ratio_step = 1.005, ratio_bounds = 20.0,
max_runtime = 3.0, bounds_scale = 0.5, stepping_scale = 0.75,
heat_capacity_cache = 0.0;
vector<gas_type> active_gases;
string rotator = "|/-\\";
size_t sample_rounds = 3;
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

long long iters = 0;
chrono::time_point start_time(main_clock.now());

dyn_val optimise_val = {float_val, &radius};
vector<shared_ptr<base_restriction>> pre_restrictions;
vector<shared_ptr<base_restriction>> post_restrictions;

bool restrictions_met(vector<shared_ptr<base_restriction>>& restrictions) {
    for (shared_ptr<base_restriction>& r : restrictions) {
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
        stream.setstate(ios_base::failbit);
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
        stream.setstate(ios_base::failbit);
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
    cout << "TICK: " << tick << " || Status: pressure " << get_pressure() << "kPa \\ integrity " << integrity << " \\ temperature " << temperature << "K\n_contents: ";
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
    vector<float> mix_ratios, primer_ratios;
    float fuel_temp, fuel_pressure, thir_temp, mix_pressure, mix_temp;
    vector<gas_type> mix_gases, primer_gases;
    tank_state state = intact;
    float radius = 0.0, fin_temp = -1.0, fin_pressure = -1.0, fin_heat_leak = -1.0, optstat = -1.0;
    int ticks = -1;

    bomb_data(vector<float> mix_ratios, vector<float> primer_ratios, float fuel_temp, float fuel_pressure, float thir_temp, float mix_pressure, float mix_temp, const vector<gas_type>& mix_gases, const vector<gas_type>& primer_gases):
        mix_ratios(mix_ratios), primer_ratios(primer_ratios), fuel_temp(fuel_temp), fuel_pressure(fuel_pressure), thir_temp(thir_temp), mix_pressure(mix_pressure), mix_temp(mix_temp), mix_gases(mix_gases), primer_gases(primer_gases) {};

    void results(float n_radius, float n_fin_temp, float n_fin_pressure, float n_optstat, int n_ticks, tank_state n_state) {
        radius = n_radius;
        fin_temp = n_fin_temp;
        fin_pressure = n_fin_pressure;
        optstat = n_optstat;
        ticks = n_ticks;
        state = n_state;
    }

    string print_very_simple() const {
        // optstat guaranteed to come first in the format `os=<number> `
        string out = "os=" + to_string(optstat) + " ti=" + to_string(ticks);
        out += " ft=" + to_string(fuel_temp) + " fp=" + to_string(fuel_pressure);
        out += " mp=" + to_string(mix_pressure) + " mt=" + to_string(mix_temp);
        out += " tt=" + to_string(thir_temp);
        out += " mi=";
        float total_mix_ratio = 0;
        for(float r : mix_ratios) total_mix_ratio += r;
        for(size_t i = 0; i < mix_gases.size(); ++i) {
            out += mix_gases[i].name() + ":" + to_string(mix_ratios[i] / total_mix_ratio) + (i == mix_gases.size() - 1 ? "" : ",");
        }
        out += " pm=";
        float total_primer_ratio = 0;
        for(float r : primer_ratios) total_primer_ratio += r;
        for(size_t i = 0; i < primer_gases.size(); ++i) {
            out += primer_gases[i].name() + ":" + to_string(primer_ratios[i] / total_primer_ratio) + (i == primer_gases.size() - 1 ? "" : ",");
        }
        return out;
    }

    string print_simple() const {
        string mix_gas_str;
        float total_mix_ratio = 0;
        for(float r : mix_ratios) total_mix_ratio += r;
        for(size_t i = 0; i < mix_gases.size(); ++i) {
            mix_gas_str += mix_gases[i].name() + " " + to_string(100.f * mix_ratios[i] / total_mix_ratio) + "%" + (i == mix_gases.size() - 1 ? "" : " | ");
        }

        string primer_gas_str;
        float total_primer_ratio = 0;
        for(float r : primer_ratios) total_primer_ratio += r;
        for(size_t i = 0; i < primer_gases.size(); ++i) {
            primer_gas_str += primer_gases[i].name() + " " + to_string(100.f * primer_ratios[i] / total_primer_ratio) + "%" + (i == primer_gases.size() - 1 ? "" : " | ");
        }

        return string(
        "TANK: { " ) +
            "mix: [ " + mix_gas_str + " | " +
                "temp " + to_string(fuel_temp) + "K | " +
                "pressure " + to_string(fuel_pressure) + "kPa " +
            "]; " +
            "primer: [ " + primer_gas_str + " | " +
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
        float total_mix_ratio = 0;
        for(float r : mix_ratios) total_mix_ratio += r;
        float total_primer_ratio = 0;
        for(float r : primer_ratios) total_primer_ratio += r;

        string initial_state_gases;
        for(size_t i = 0; i < mix_gases.size(); ++i) {
            initial_state_gases += "\t\t" + mix_gases[i].name() + "\t" + to_string(pressure_temp_to_mols(mix_ratios[i] / total_mix_ratio * fuel_pressure, fuel_temp)) + " mol\n";
        }
        for(size_t i = 0; i < primer_gases.size(); ++i) {
            initial_state_gases += "\t\t" + primer_gases[i].name() + "\t" + to_string(pressure_temp_to_mols(primer_ratios[i] / total_primer_ratio * (pressure_cap - fuel_pressure), thir_temp)) + " mol\n";
        }

        string reqs_mix_canister_ratios;
        string reqs_mix_canister_least_mols_names;
        string reqs_mix_canister_least_mols_vals;
        float volume_ratio = (required_transfer_volume + volume) / volume;
        for(size_t i = 0; i < mix_gases.size(); ++i) {
            reqs_mix_canister_ratios += mix_gases[i].name() + (i == mix_gases.size() - 1 ? "" : "\t");
        }
        reqs_mix_canister_ratios += "\n\t\tgas ratio\t";
        for(size_t i = 0; i < mix_gases.size(); ++i) {
            reqs_mix_canister_ratios += to_string(100.f * mix_ratios[i] / total_mix_ratio) + "%" + (i == mix_gases.size() - 1 ? "" : "\t");
        }
        for(size_t i = 0; i < mix_gases.size(); ++i) {
            reqs_mix_canister_least_mols_names += mix_gases[i].name() + (i == mix_gases.size() - 1 ? "" : "\t\t");
            reqs_mix_canister_least_mols_vals += to_string(pressure_temp_to_mols(mix_ratios[i] / total_mix_ratio * fuel_pressure, fuel_temp) * volume_ratio) + (i == mix_gases.size() - 1 ? "" : "\t");
        }

        string reqs_primer_canister;
        float added_ratio = (required_transfer_volume + volume) / required_transfer_volume;
        if (!primer_gases.empty()) {
            reqs_primer_canister += "\tthird-canister (primer): [\n";
            if(primer_gases.size() > 1) {
                reqs_primer_canister += "\t\tgas ratio\t";
                for(size_t i = 0; i < primer_gases.size(); ++i) reqs_primer_canister += primer_gases[i].name() + (i == primer_gases.size() - 1 ? "" : "\t");
                reqs_primer_canister += "\n\t\tgas ratio\t";
                for(size_t i = 0; i < primer_gases.size(); ++i) reqs_primer_canister += to_string(100.f * primer_ratios[i] / total_primer_ratio) + "%" + (i == primer_gases.size() - 1 ? "" : "\t");
                reqs_primer_canister += "\n";
            }
            reqs_primer_canister += "\t\ttemperature\t" + to_string(thir_temp) + " K\n" +
                "\t\tpressure\t" + to_string((pressure_cap * 2.0 - fuel_pressure) * added_ratio) + " kPa\n" +
                "\t\tleast-mols:\t" + to_string(pressure_temp_to_mols(pressure_cap * 2.0 - fuel_pressure, thir_temp) * volume_ratio) + " mol\t";
            for(size_t i = 0; i < primer_gases.size(); ++i) reqs_primer_canister += primer_gases[i].name() + (i == primer_gases.size() - 1 ? "" : ",");
            reqs_primer_canister += "\n\t]\n";
        }

        return string(
        "TANK: {\n" ) +
            "\tinitial state: [\n" +
                "\t\ttemperature\t" + to_string(mix_temp) + " K\n" +
                "\t\tpressure\t" + to_string(mix_pressure) + " kPa\n" +
                initial_state_gases +
            "\t];\n" +
            "\tend state: [\n" +
                "\t\ttime\t\t" + to_string(ticks * tickrate) + " s\n" +
                "\t\tpressure \t" + to_string(fin_pressure) + " kPa\n" +
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
                "\t\tgas ratio\t" + reqs_mix_canister_ratios + "\n" +
                "\t\ttemperature\t" + to_string(fuel_temp) + " K\n" +
                "\t\ttank pressure\t" + to_string(fuel_pressure) + " kPa\n" +
                "\t\tleast-mols: [\n" +
                    "\t\t\t" + reqs_mix_canister_least_mols_names + "\n" +
                    "\t\t\t" + reqs_mix_canister_least_mols_vals + "\n" +
                "\t\t]\n"
            "\t];\n" +
            reqs_primer_canister +
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

float optimise_stat() {
    return optimise_val.type == float_val ? get_dyn<float>(optimise_val) : get_dyn<int>(optimise_val);
}

bomb_data get_data(const vector<float>&, tuple<const vector<gas_type>&, const vector<gas_type>&, bool>);

template<typename T, typename R>
struct optimizer {
    function<R(const vector<float>&, T)> funct;
    tuple<const vector<float>&, T> args;
    const vector<float>& lower_bounds;
    const vector<float>& upper_bounds;
    const vector<float>& min_lin_step;
    const vector<float>& min_exp_step;
    bool maximise;
    chrono::duration<float> max_duration;

    vector<float> current;
    vector<float> best_arg;
    R best_result;
    bool any_valid = false;

    float step_scale = 1.f;

    optimizer(function<R(const vector<float>&, T)> func,
              const vector<float>& lowerb,
              const vector<float>& upperb,
              const vector<float>& i_lin_step,
              const vector<float>& i_exp_step,
              bool maxm,
              T i_args,
              chrono::duration<float> i_max_duration = chrono::duration<float>(max_runtime)) :
              funct(func),
              args(current, i_args),
              lower_bounds(lowerb),
              upper_bounds(upperb),
              min_lin_step(i_lin_step),
              min_exp_step(i_exp_step),
              maximise(maxm),
              max_duration(i_max_duration) {

        size_t misize = std::min({lowerb.size(), upperb.size(), i_lin_step.size(), i_exp_step.size()});
        size_t masize = std::max({lowerb.size(), upperb.size(), i_lin_step.size(), i_exp_step.size()});
        if (misize != masize) {
            throw runtime_error("optimiser parameters have mismatched dimensions");
        }

        for (size_t i = 0; i < masize; ++i) {
            if (lowerb[i] > upperb[i]) {
                throw runtime_error("optimiser upper bound " + to_string(i) + " was smaller than lower bound");
            }
        }

        current = lower_bounds; // copy

        best_arg = vector<float>(current.size(), 0.f);

        best_result = worst_res();
    }

    void find_best() {
        size_t paramc = current.size();
        vector<float> cur_lower_bounds = lower_bounds;
        vector<float> cur_upper_bounds = upper_bounds;
        step_scale = 1.f;
        for (size_t samp_idx = 0; samp_idx < sample_rounds; ++samp_idx) {
            chrono::time_point s_time = main_clock.now();
            while (main_clock.now() - s_time < max_duration) {
                // start off a random point in the dimension space
                for (size_t i = 0; i < current.size(); ++i) {
                    current[i] = cur_lower_bounds[i] + (cur_upper_bounds[i] - cur_lower_bounds[i]) * frand();
                }
                // do gradient descent until we find a local minimum
                R c_result = worst_res();
                while (true) {
                    if (log_level >= 3) {
                        cout << "Sampling: ";
                        for (float f : current) {
                            cout << f << " ";
                        }
                        cout << endl;
                    }
                    // movement directions yielding best result
                    vector<pair<size_t, bool>> best_movedirs = {};
                    R best_move_res = c_result;
                    vector<float> old_current = current;
                    // sample each possible movement direction in the parameter space
                    for (size_t i = 0; i < paramc; ++i) {
                        auto do_update = [&](const R& with, bool sign) {
                            if (with == best_move_res) {
                                best_movedirs.push_back({i, sign});
                            } else if (better_than(with, best_move_res)) {
                                best_movedirs = {{i, sign}};
                                best_move_res = with;
                            }
                        };
                        // step forward in this dimension
                        current[i] += get_step(i);
                        if (current[i] <= cur_upper_bounds[i]) {
                            R res = sample();
                            do_update(res, true);
                        }
                        // reset and step backwards
                        current[i] = old_current[i];
                        current[i] -= get_step(i);
                        if (current[i] >= cur_lower_bounds[i]) {
                            R res = sample();
                            do_update(res, false);
                        }
                        // reset
                        current[i] = old_current[i];
                    }

                    // found local minimum
                    if (best_movedirs.empty()) {
                        if (log_level >= 2) {
                            bomb_data data = get_data(get<0>(args), get<1>(args));
                            print_bomb(data, "Local minimum found: ");
                        }
                        break;
                    }

                    auto do_skipmove = [&](size_t dir, float sign) {
                        size_t chosen_scl = 0;
                        for (size_t move_scl = 2; true; move_scl *= 2) {
                            // step forward in chosen direction with scaling
                            current[dir] += sign * get_step(dir, move_scl);
                            // don't try to sample beyond bounds
                            if (current[dir] < cur_lower_bounds[dir] || current[dir] > cur_upper_bounds[dir]) {
                                // reset
                                current[dir] = old_current[dir];
                                break;
                            }
                            // sample and check if scaling the movement produced better or same results
                            R res = sample();
                            if (better_eq_than(res, best_move_res)) {
                                chosen_scl = move_scl;
                                best_move_res = res;
                            } else {
                                current[dir] = old_current[dir];
                                break;
                            }
                            current[dir] = old_current[dir];
                        }
                        return chosen_scl;
                    };
                    pair<size_t, float> chosen;
                    size_t chosen_scl = 0;
                    // try skip-move in each prospective movement direction
                    for (const pair<size_t, bool>& p : best_movedirs) {
                        float sign = p.second ? +1.f : -1.f;
                        // check if we can skip-move in this direction
                        // the function handles checking whether that'd actually be profitable
                        size_t scl = do_skipmove(p.first, sign);
                        if (scl != 0) {
                            chosen = {p.first, sign};
                            chosen_scl = scl;
                        }
                    }
                    // we failed to find any non-zero movement, break to avoid random walk
                    if (chosen_scl == 0 || best_move_res == c_result) {
                        if (log_level >= 2) {
                            bomb_data data = get_data(get<0>(args), get<1>(args));
                            print_bomb(data, "Local minimum found: ");
                        }
                        break;
                    }
                    // perform the movement
                    current[chosen.first] += chosen.second * get_step(chosen.first, chosen_scl);
                    c_result = best_move_res;
                }
            }
            if (!any_valid) {
                if (log_level >= 1) {
                    cout << "Failed to find any viable bomb, retrying sample 1..." << endl;
                }
                --samp_idx;
                s_time = main_clock.now();
                continue;
            }
            if (samp_idx + 1 != sample_rounds) {
                if (log_level >= 1) {
                    cout << "\nSampling round " << samp_idx + 1 << " complete" << endl;
                    bomb_data data = get_data(best_arg, get<1>(args));
                    print_bomb(data, "Best bomb found: ");
                }
                // sampling round done, halve sampling area and go again
                for (size_t i = 0; i < current.size(); ++i) {
                    float& lowerb = cur_lower_bounds[i];
                    float& upperb = cur_upper_bounds[i];
                    const float& best_at = best_arg[i];
                    lowerb += (best_at - lowerb) * (1.f - bounds_scale);
                    upperb += (best_at - upperb) * (1.f - bounds_scale);
                    // scale stepping less
                    step_scale *= stepping_scale;
                }
                if (log_level >= 1) {
                    cout << "New bounds: ";
                    for (size_t i = 0; i < current.size(); ++i) {
                        cout << "[" << cur_lower_bounds[i] << "," << cur_upper_bounds[i] << "] ";
                    }
                    cout << endl;
                }
            }
        }
    }

    // returns pair of sign-adjusted result and whether this updated our maximum
    R sample() {
        R tres = apply(funct, args);
        bool valid = tres.valid();
        R res = valid ? tres : worst_res();
        any_valid |= valid;

        if (better_than(res, best_result)) {
            best_result = res;
            best_arg = current;
        }

        return res;
    }

    bool better_than(R what, R than) {
        return maximise ? what > than : than > what;
    }

    bool better_eq_than(R what, R than) {
        return maximise ? what >= than : than >= what;
    }

    R worst_res() {
        return R::worst(maximise);
    }

    float get_step(int i, float scale = 1.f) {
        scale *= step_scale;
        const float& c_param = current[i];
        const float& min_l_step = min_lin_step[i];
        const float& min_e_step = min_exp_step[i];
        float step = std::max(c_param * (1.f + (min_e_step - 1.f) * scale), c_param + min_l_step * scale) - c_param;
        return step;
    }

    void step(int i) {
        current[i] += get_step(i);
    }
};

struct opt_val_wrap {
    float stat;
    float pressure;
    bool valid_v;

    static opt_val_wrap worst(bool maximise) {
        float w = numeric_limits<float>::max() * (maximise ? -1.f : 1.f);
        return {w, w, false};
    }

    bool valid() const {
        return valid_v;
    }

    bool operator>(const opt_val_wrap& rhs) const {
        return stat == rhs.stat ? pressure > rhs.pressure : stat > rhs.stat;
    }

    bool operator>=(const opt_val_wrap& rhs) const {
        return stat >= rhs.stat;
    }

    bool operator==(const opt_val_wrap& rhs) const {
        return stat == rhs.stat;
    }
};

// args: target_temp, fuel_temp, thir_temp, mix ratios..., primer ratios...
opt_val_wrap do_sim(const vector<float>& in_args, tuple<const vector<gas_type>&, const vector<gas_type>&, bool> args) {
    float target_temp = in_args[0];
    float fuel_temp = in_args[1];
    float thir_temp = in_args[2];
    const vector<gas_type>& mix_gases = get<0>(args);
    const vector<gas_type>& primer_gases = get<1>(args);
    bool measure_before = get<2>(args);
    ++iters;
    if (iters % progress_bar_spacing == 0) {
        print_progress(iters, start_time);
    }
    float fuel_pressure, stat;
    reset();
    if ((target_temp > fuel_temp) == (target_temp > thir_temp)) {
        return {0.f, 0.f, false};
    }
    vector<float> mix_ratios(mix_gases.size(), 1.f);
    for (size_t i = 0; i < mix_gases.size() - 1; ++i) {
        mix_ratios[i + 1] = in_args[3 + i];
    }
    vector<float> primer_ratios(primer_gases.size(), 1.f);
    size_t mg_s = mix_gases.size() - 1;
    for (size_t i = 0; i < primer_gases.size() - 1; ++i) {
        primer_ratios[i + 1] = in_args[3 + mg_s + i];
    }
    fuel_pressure = mix_input_setup(mix_gases, mix_ratios, primer_gases, primer_ratios, fuel_temp, thir_temp, target_temp);
    if (fuel_pressure > pressure_cap || fuel_pressure < 0.0) {
        return {0.f, 0.f, false};
    }
    bool pre_met = restrictions_met(pre_restrictions);
    if (measure_before) {
        stat = optimise_stat();
    }
    loop();
    if (!measure_before) {
        stat = optimise_stat();
    }
    if (!pre_met || !restrictions_met(post_restrictions)) {
        return {0.f, get_pressure(), false};
    }
    return {stat, get_pressure(), true};
}

bomb_data get_data(const vector<float>& in_args, tuple<const vector<gas_type>&, const vector<gas_type>&, bool> args) {
    float fuel_pressure, stat;
    float target_temp = in_args[0];
    float fuel_temp = in_args[1];
    float thir_temp = in_args[2];
    const vector<gas_type>& mix_gases = get<0>(args);
    const vector<gas_type>& primer_gases = get<1>(args);
    bool measure_before = get<2>(args);
    reset();
    vector<float> mix_ratios(mix_gases.size(), 1.f);
    for (size_t i = 0; i < mix_gases.size() - 1; ++i) {
        mix_ratios[i + 1] = in_args[3 + i];
    }
    vector<float> primer_ratios(primer_gases.size(), 1.f);
    size_t mg_s = mix_gases.size() - 1;
    for (size_t i = 0; i < primer_gases.size() - 1; ++i) {
        primer_ratios[i + 1] = in_args[3 + mg_s + i];
    }
    fuel_pressure = mix_input_setup(mix_gases, mix_ratios, primer_gases, primer_ratios, fuel_temp, thir_temp, target_temp);
    if (measure_before) {
        stat = optimise_stat();
    }
    float mix_pressure = get_pressure();
    loop();
    if (!measure_before) {
        stat = optimise_stat();
    }
    bomb_data bomb(mix_ratios, primer_ratios, fuel_temp, fuel_pressure, thir_temp, mix_pressure, target_temp, mix_gases, primer_gases);
    bomb.results(radius, temperature, get_pressure(), stat, tick, cur_state);
    return bomb;
}

bomb_data test_mix(const vector<gas_type>& mix_gases, const vector<gas_type>& primer_gases, float mixt1, float mixt2, float thirt1, float thirt2, bool maximise, bool measure_before) {
    size_t num_mix_ratios = mix_gases.size() > 1 ? mix_gases.size() - 1 : 0;
    size_t num_primer_ratios = primer_gases.size() > 1 ? primer_gases.size() - 1 : 0;
    size_t num_params = 3 + num_mix_ratios + num_primer_ratios;

    vector<float> lower_bounds = {std::min(mixt1, thirt1), mixt1, thirt1};
    lower_bounds[0] = std::max(lower_target_temp, lower_bounds[0]);
    vector<float> upper_bounds = {std::max(mixt2, thirt2), mixt2, thirt2};
    if (!step_target_temp) {
        upper_bounds[0] = lower_bounds[0];
    }
    for (size_t i = 0; i < num_params - 3; ++i) {
        lower_bounds.push_back(1.f / ratio_bounds);
        upper_bounds.push_back(ratio_bounds);
    }

    vector<float> min_l_step(lower_bounds.size(), 0.f);
    vector<float> min_e_step(lower_bounds.size(), 0.f);
    for (size_t i = 0; i < 3; ++i) {
        min_l_step[i] = temperature_step_min;
        min_e_step[i] = temperature_step;
    }
    for (size_t i = 3; i < num_params; ++i) {
        min_l_step[i] = 0.f;
        min_e_step[i] = ratio_step;
    }

    optimizer<tuple<const vector<gas_type>&, const vector<gas_type>&, bool>, opt_val_wrap> optim(do_sim, lower_bounds, upper_bounds, min_l_step, min_e_step, maximise, make_tuple(ref(mix_gases), ref(primer_gases), measure_before));

    optim.find_best();

    vector<float> in_args = optim.best_arg;
    return get_data(in_args, make_tuple(ref(mix_gases), ref(primer_gases), measure_before));
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

    vector<gas_type> mix_gases;
    vector<gas_type> primer_gases;
    float mixt1 = 0.0, mixt2 = 0.0, thirt1 = 0.0, thirt2 = 0.0;

    bool redefine_heatcap = false, mixing_mode = false, manual_mix = false, do_retest = false;
    tuple<dyn_val, bool, bool> opt_param = {{float_val, &radius}, true, false};

    std::vector<std::shared_ptr<argp::base_argument>> args = {
        argp::make_argument("pipeonly", "", "assume inside pipe: prevent tank-related effects", check_status),
        argp::make_argument("redefineheatcap", "", "redefine heat capacities", redefine_heatcap),
        argp::make_argument("ratiobounds", "", "set gas ratio iteration bounds", ratio_bounds),
        argp::make_argument("mixtoiter", "s", "provide potentially better results by also iterating the mix-to temperature (WARNING: will take many times longer to calculate)", step_target_temp),
        argp::make_argument("mixingmode", "m", "UTILITY TOOL: utility to find desired mixer percentage if mixing different-temperature gases", mixing_mode),
        argp::make_argument("manualmix", "f", "UTILITY TOOL: manually input a tank's contents and simulate it", manual_mix),
        argp::make_argument("mixg", "mg", "list of fuel gases (usually, in tank)", mix_gases),
        argp::make_argument("primerg", "pg", "list of primer gases (usually, in canister)", primer_gases),
        argp::make_argument("mixt1", "m1", "minimum fuel mix temperature to check, Kelvin", mixt1),
        argp::make_argument("mixt2", "m2", "maximum fuel mix temperature to check, Kelvin", mixt2),
        argp::make_argument("thirt1", "t1", "minimum primer mix temperature to check, Kelvin", thirt1),
        argp::make_argument("thirt2", "t2", "maximum primer mix temperature to check, Kelvin", thirt2),
        argp::make_argument("doretest", "", "after calculating the bomb, test it again and print every tick as it reacts", do_retest),
        argp::make_argument("ticks", "t", "set tick limit: aborts if a bomb takes longer than this to detonate (default: " + to_string(tick_cap) + ")", tick_cap),
        argp::make_argument("tstep", "", "set temperature iteration multiplier (default " + to_string(temperature_step) + ")", temperature_step),
        argp::make_argument("tstepm", "", "set minimum temperature iteration step (default " + to_string(temperature_step_min) + ")", temperature_step_min),
        argp::make_argument("volume", "v", "set tank volume (default " + to_string(volume) + ")", volume),
        argp::make_argument("lowertargettemp", "o", "only consider bombs which mix to above this temperature; higher values may make bombs more robust to slight mismixing (default " + to_string(lower_target_temp) + ")", lower_target_temp),
        argp::make_argument("loglevel", "l", "what level of the nested loop to log, 0-6: none, [default] global_best, thir_temp, fuel_temp, target_temp, all, debug", log_level),
        argp::make_argument("param", "p", "(param, maximise, measure_before_sim): lets you configure what parameter and how to optimise", opt_param),
        argp::make_argument("restrictpre", "rb", "lets you make atmosim not consider bombs outside of chosen parameters, measured before simulation", pre_restrictions),
        argp::make_argument("restrictpost", "ra", "same as -rr, but measured after simulation", post_restrictions),
        argp::make_argument("simpleout", "", "makes very simple output, for use by other programs or advanced users", simple_output),
        argp::make_argument("silent", "", "output ONLY the final result, overrides loglevel", silent),
        argp::make_argument("runtime", "rt", "for how long to run in seconds (default " + to_string(max_runtime) + ")", max_runtime),
        argp::make_argument("samplerounds", "sr", "how many sampling rounds to perform, multiplies runtime (default " + to_string(sample_rounds) + ")", sample_rounds),
        argp::make_argument("boundsscale", "", "how much to scale bounds each sample round (default " + to_string(bounds_scale) + ")", bounds_scale),
        argp::make_argument("steppingscale", "", "how much to scale minimum step each sample round (default " + to_string(stepping_scale) + ")", stepping_scale)
    };

    argp::parse_arguments(args, argc, argv,
    // pre-help
        "Atmosim: SS14 atmos maxcap calculator utility\n"
        "  This program contains an optimisation algorithm that attempts to find the best bomb possible according to the desired parameters.\n"
        "  Additionally, there's a few extra utility tools you can activate instead of the primary mode with their respective flags.\n"
        "\n"
        "  Available parameter types:\n"
        "    " + list_params() +
        "\n"
        "  Available gas types:\n"
        "    " + list_gases() +
        "\n",
    // post-help
        "\n"
        "Example usage:\n"
        "  `./atmosim -mg=[plasma,tritium] -pg=[oxygen] -m1=375.15 -m2=595.15 -t1=293.15 -t2=293.15 -s`\n"
        "  This should find you a ~13.2 radius maxcap recipe. Experiment with other parameters.\n"
        "  For --restrictpre (-rb) and --restrictpost (-ra):\n"
        "  `./atmosim -mg=[plasma,tritium] -pg=[oxygen] -m1=375.15 -m2=595.15 -t1=293.15 -t2=293.15 -s -ra=[[radius,0,11],[ticks,20,44]]`\n"
        "\n"
        "Tips and tricks\n"
        "  Using the -s flag may produce considerably better results if you're willing to wait.\n"
        "  Additionally, consider tuning down amplif, part of the utility's optimiser, with the --maxamplif flag.\n"
        "  Consider restricting temperature ranges if it's taking a long time to run.\n"
        "  If you want a long-fuse bomb, try using the -p flag to optimise to maximise ticks and the -r flag to restrict radius to be above a desired value.\n"
        "  Remember to use the -t flag to raise maximum alotted ticks if you're finding long-fuse bombs.\n"
        "\n"
        "  Brought to you by Ilya246 and friends"
    );

    optimise_val = get<0>(opt_param);
    optimise_maximise = get<1>(opt_param);
    optimise_before = get<2>(opt_param);

    if (redefine_heatcap) {
        heat_cap_input_setup();
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
    // didn't exit prior, test n gas -> k-gas-mix tanks
    if ((mix_gases.empty() || primer_gases.empty()) && !silent) {
        cout << "No mix or primer gases found, see `./atmosim -h` for usage\n";
        cout << "Gases: " << list_gases() << endl;
        return 0;
    }

    bomb_data best_bomb = test_mix(mix_gases, primer_gases, mixt1, mixt2, thirt1, thirt2, optimise_maximise, optimise_before);
    cout.clear();
    cout << (simple_output ? "" : "Best:\n") << (simple_output ? best_bomb.print_very_simple() : best_bomb.print_extensive()) << endl;
    if (silent) {
        cout.setstate(ios::failbit);
    }
    if (do_retest) {
        reset();
        known_input_setup(best_bomb.mix_gases, best_bomb.mix_ratios, best_bomb.primer_gases, best_bomb.primer_ratios, best_bomb.fuel_temp, best_bomb.thir_temp, best_bomb.fuel_pressure);
        loop_print();
    }
    return 0;
}

#include <chrono>
#include <cmath>
#include <format>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <argparse/args.hpp>
#include <argparse/read.hpp>

#include "constants.hpp"
#include "optimiser.hpp"
#include "gas.hpp"
#include "tank.hpp"

using namespace std;
using namespace asim;

struct bomb_data {
    vector<float> mix_ratios, primer_ratios;
    float fuel_temp, fuel_pressure, thir_temp, mix_to_pressure, mix_to_temp;
    vector<gas_ref> mix_gases, primer_gases;
    gas_tank tank;
    float radius, pressure, optstat;
    int ticks;

    // TODO: make this more sane somehow?
    bomb_data(vector<float> mix_ratios, vector<float> primer_ratios,
              float fuel_temp, float fuel_pressure, float thir_temp, float mix_to_pressure, float mix_to_temp,
              const vector<gas_ref>& mix_gases, const vector<gas_ref>& primer_gases,
              gas_tank tank, float radius = -1.f, float pressure = -1.f, float optstat = -1.f,
              int ticks = -1)
    :
        mix_ratios(mix_ratios), primer_ratios(primer_ratios),
        fuel_temp(fuel_temp), fuel_pressure(fuel_pressure), thir_temp(thir_temp), mix_to_pressure(mix_to_pressure), mix_to_temp(mix_to_temp),
        mix_gases(mix_gases), primer_gases(primer_gases),
        tank(tank),
        radius(radius), pressure(pressure), optstat(optstat),
        ticks(ticks) {};

    void set_state(float i_radius, float i_pressure, float i_optstat, int i_ticks) {
        radius = i_radius;
        pressure = i_pressure;
        optstat = i_optstat;
        ticks = i_ticks;
    }

    string mix_string(const vector<gas_ref>& gases, const vector<float>& fractions) const {
        string out;
        for (size_t i = 0; i < gases.size(); ++i) {
            out += format("{}% {}", fractions[i] * 100.f, gases[i].name());
            if (i != gases.size() - 1) out += " | ";
        }
        return out;
    }

    string mix_string_simple(const vector<gas_ref>& gases, const vector<float>& fractions) const {
        string out;
        for (size_t i = 0; i < gases.size(); ++i) {
            out += format("{}:{}", gases[i].name(), fractions[i]);
            if (i != gases.size() - 1) out += ",";
        }
        return out;
    }

    string print_very_simple() const {
        string out_str;
        vector<float> mix_fractions = get_fractions(mix_ratios);
        vector<float> primer_fractions = get_fractions(primer_ratios);
        // note: this format is supposed to be script-friendly and backwards-compatible
        out_str += format("os={} ti={} ft={} fp={} mp={} mt={} tt={} mi={} pm={}", optstat, ticks, fuel_temp, fuel_pressure, mix_to_pressure, mix_to_temp, thir_temp, mix_string_simple(mix_gases, mix_fractions), mix_string_simple(primer_gases, primer_fractions));
        return out_str;
    }

    string print_inline() const {
        string out_str;

        vector<float> mix_fractions = get_fractions(mix_ratios);
        vector<float> primer_fractions = get_fractions(primer_ratios);

        float required_primer_p = pressure_cap + (pressure_cap - fuel_pressure);

        out_str += format("S: [ time {:.1f}s | radius {:.2f}til | optstat {} ] ", ticks * tickrate, radius, optstat);
        out_str += format("M: [ {} | {}K | {}kPa ] ", mix_string(mix_gases, mix_fractions), fuel_temp, fuel_pressure);
        out_str += format("C: [ {} | {}K | >{}kPa ]", mix_string(primer_gases, primer_fractions), thir_temp, required_primer_p);

        return out_str;
    }

    string print_full() const {
        string out_str;

        vector<float> mix_fractions = get_fractions(mix_ratios);
        vector<float> primer_fractions = get_fractions(primer_ratios);
        size_t mix_c = mix_gases.size(), primer_c = primer_gases.size(), total_c = mix_c + primer_gases.size();

        vector<pair<float, string>> min_amounts(mix_gases.size() + primer_gases.size());
        float required_volume = (required_transfer_volume + tank.mix.volume);
        for (size_t i = 0; i < mix_c; ++i) {
            min_amounts[i] = {to_mols(mix_fractions[i] * fuel_pressure, required_volume, fuel_temp), (string)mix_gases[i].name()};
        }
        float required_primer_p = pressure_cap + (pressure_cap - fuel_pressure);
        required_primer_p *= required_volume / required_transfer_volume;
        for (size_t i = 0; i < primer_c; ++i) {
            min_amounts[i + mix_c] = {to_mols(primer_fractions[i] * required_primer_p, required_volume, thir_temp), (string)primer_gases[i].name()};
        }
        string req_str;
        for (size_t i = 0; i < total_c; ++i) {
            req_str += format("{:.2f}mol {}", min_amounts[i].first, min_amounts[i].second);
            if (i + 1 != total_c) req_str += " | ";
        }

        out_str += format("STATS: [ time {:.1f}s | radius {:.2f}til | optstat {} ]\n", ticks * tickrate, radius, optstat);
        out_str += format("MIX:   [ {} | {}K | {}kPa ]\n", mix_string(mix_gases, mix_fractions), fuel_temp, fuel_pressure);
        out_str += format("CAN:   [ {} | {}K | >{}kPa ]\n", mix_string(primer_gases, primer_fractions), thir_temp, required_primer_p);
        out_str += format("REQ:   [ {} ]", req_str);

        return out_str;
    }
};

// wrapper for bomb_data for use by the optimiser
struct opt_val_wrap {
    shared_ptr<bomb_data> data = nullptr;
    bool valid_v = true;

    opt_val_wrap(): valid_v(false) {}
    opt_val_wrap(shared_ptr<bomb_data>& d): data(d), valid_v(d != nullptr) {}

    static const opt_val_wrap worst(bool) {
        return {};
    }

    bool valid() const {
        return valid_v;
    }

    bool operator>(const opt_val_wrap& rhs) const {
        return data->optstat == rhs.data->optstat ? data->pressure > rhs.data->pressure : data->optstat > rhs.data->optstat;
    }

    bool operator>=(const opt_val_wrap& rhs) const {
        return data->optstat >= rhs.data->optstat;
    }

    bool operator==(const opt_val_wrap& rhs) const {
        return data->optstat == rhs.data->optstat;
    }
};

template<typename T>
struct field_ref {
    enum field_type {invalid_f, int_f, float_f};

    size_t offset;
    field_type type = invalid_f;

    float get(T& from) {
        switch (type) {
            case (float_f): return *(float*)((char*)&from + offset);
            case (int_f): return *(int*)((char*)&from + offset);
            default: throw runtime_error("tried getting value of invalid field reference");
        }
    }
};

// args: target_temp, fuel_temp, thir_temp, mix ratios..., primer ratios...
opt_val_wrap do_sim(const vector<float>& in_args, tuple<const vector<gas_ref>&, const vector<gas_ref>&, bool, size_t, field_ref<gas_tank>> args) {
    // read input parameters
    float target_temp = in_args[0];
    float fuel_temp = in_args[1];
    float thir_temp = in_args[2];
    // invalid mix, abort early
    if ((target_temp > fuel_temp) == (target_temp > thir_temp)) {
        return {};
    }
    const vector<gas_ref>& mix_gases = get<0>(args);
    const vector<gas_ref>& primer_gases = get<1>(args);
    bool measure_before = get<2>(args);
    size_t tick_cap = get<3>(args);
    field_ref optstat_ref = get<4>(args);

    // read gas ratios
    vector<float> mix_ratios(mix_gases.size(), 1.f);
    vector<float> primer_ratios(primer_gases.size(), 1.f);
    size_t mg_s = mix_gases.size() - 1;
    size_t pg_s = primer_gases.size() - 1;
    for (size_t i = 0; i < mg_s; ++i) {
        mix_ratios[i + 1] = in_args[3 + i];
    }
    for (size_t i = 0; i < pg_s; ++i) {
        primer_ratios[i + 1] = in_args[3 + mg_s + i];
    }

    vector<float> mix_fractions = get_fractions(mix_ratios);
    vector<float> primer_fractions = get_fractions(primer_ratios);

    // set up the tank
    gas_tank tank;

    // specific heat is heat capacity of 1mol and fractions sum up to 1mol
    float fuel_specheat = get_mix_heat_capacity(mix_gases, mix_fractions);
    float primer_specheat = get_mix_heat_capacity(primer_gases, primer_fractions);
    // to how much we want to fill the tank
    float fuel_pressure = (target_temp / thir_temp - 1.f) * pressure_cap / (fuel_specheat / primer_specheat - 1.f + target_temp * (1.f / thir_temp - fuel_specheat / primer_specheat / fuel_temp));
    tank.mix.canister_fill_to(mix_gases, mix_fractions, fuel_temp, fuel_pressure);
    tank.mix.canister_fill_to(primer_gases, primer_fractions, thir_temp, pressure_cap);
    float mix_pressure = tank.mix.pressure();

    // invalid mix, abort
    if (fuel_pressure > pressure_cap || fuel_pressure < 0.0) {
        return {};
    }
    shared_ptr<bomb_data> bomb = make_shared<bomb_data>(mix_ratios, primer_ratios,
                   fuel_temp, fuel_pressure, thir_temp, mix_pressure, target_temp,
                   mix_gases, primer_gases,
                   tank);
    float stat = -1.f;
    // TODO critical
    // bool pre_met = restrictions_met(pre_restrictions);
    if (measure_before) stat = optstat_ref.get(tank);

    // simulate for up to tick_cap ticks
    size_t ticks = tank.tick_n(tick_cap);

    if (!measure_before) stat = optstat_ref.get(tank);
    // TODO critical
    //if (!pre_met || !restrictions_met(post_restrictions)) {
    //    return {0.f, get_pressure(), false};
    //}

    bomb->set_state(tank.radius_cache, tank.mix.pressure(), stat, ticks);
    return opt_val_wrap(bomb);
}

int main(int argc, char* argv[]) {
    size_t log_level = 2;

    bool mixing_mode = false, do_retest = false;
    bool simple_output = false, silent = false;

    vector<gas_ref> mix_gases;
    vector<gas_ref> primer_gases;
    float mixt1 = 0.f, mixt2 = 0.f, thirt1 = 0.f, thirt2 = 0.f;
    float ratio_bounds = 10.f;
    float ratio_step = 1.005f;
    float temperature_step = 1.001f, temperature_step_min = 0.05f;
    float lower_target_temp = fire_temp + 0.1f;
    bool step_target_temp = false;
    size_t tick_cap = 60;

    field_ref<gas_tank> opt_param(offsetof(gas_tank, radius_cache), field_ref<gas_tank>::float_f);

    float max_runtime = 5.f;
    size_t sample_rounds = 3;
    float bounds_scale = 0.5f;
    float stepping_scale = 0.75f;

    std::vector<std::shared_ptr<argp::base_argument>> args = {
        argp::make_argument("ratiobounds", "", "set gas ratio iteration bounds", ratio_bounds),
        argp::make_argument("mixtoiter", "s", "provide potentially better results by also iterating the mix-to temperature (WARNING: will take many times longer to calculate)", step_target_temp),
        argp::make_argument("mixingmode", "m", "UTILITY TOOL: utility to find desired mixer percentage if mixing different-temperature gases", mixing_mode),
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
        argp::make_argument("lowertargettemp", "o", "only consider bombs which mix to above this temperature; higher values may make bombs more robust to slight mismixing (default " + to_string(lower_target_temp) + ")", lower_target_temp),
        argp::make_argument("loglevel", "l", "how much to log (default " + to_string(log_level) + ")", log_level),
        // TODO critical
        // argp::make_argument("param", "p", "(param, maximise, measure_before_sim): lets you configure what parameter and how to optimise", opt_param),
        // argp::make_argument("restrictpre", "rb", "lets you make atmosim not consider bombs outside of chosen parameters, measured before simulation", pre_restrictions),
        // argp::make_argument("restrictpost", "ra", "same as -rr, but measured after simulation", post_restrictions),
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
        // TODO critical
        // "  Available parameter types:\n"
        // "    " + list_params() +
        // "\n"
        "  Available gas types:\n"
        "    " + list_gases() +
        "\n",
    // post-help
        "\n"
        "Example usage:\n"
        "  `./atmosim -mg=[plasma,tritium] -pg=[oxygen] -m1=375.15 -m2=595.15 -t1=293.15 -t2=293.15 -s`\n"
        "  This should find you a ~13.5 radius maxcap recipe. Experiment with other parameters.\n"
        "  For --restrictpre (-rb) and --restrictpost (-ra):\n"
        "  `./atmosim -mg=[plasma,tritium] -pg=[oxygen] -m1=375.15 -m2=595.15 -t1=293.15 -t2=293.15 -s -ra=[[radius,0,11],[ticks,20,44]]`\n"
        "\n"
        "Tips and tricks\n"
        "  Consider using the -s flag for radius-optimised bombs. Not recommended for ticks-optimised bombs.\n"
        "  Additionally, consider letting the optimiser think for longer using the -rt and -sr flags.\n"
        "  If you want a long-fuse bomb, try using the -p flag to optimise to maximise ticks and the -r flag to restrict radius to be above a desired value.\n"
        "  Remember to use the -t flag to raise maximum alotted ticks if you're trying to find long-fuse bombs.\n"
        "\n"
        "  Brought to you by Ilya246 and friends"
    );

    // TODO critical
    // optimise_val = get<0>(opt_param);
    // optimise_maximise = get<1>(opt_param);
    // optimise_before = get<2>(opt_param);
    bool optimise_measure_before = false;
    bool optimise_maximise = true;

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

    optimiser<tuple<vector<gas_ref>, vector<gas_ref>, bool, size_t, field_ref<gas_tank>>, opt_val_wrap>
    optim(do_sim,
          lower_bounds,
          upper_bounds,
          min_l_step,
          min_e_step,
          optimise_maximise,
          make_tuple(mix_gases, primer_gases, optimise_measure_before, tick_cap, opt_param),
          chrono::duration<float>(max_runtime),
          sample_rounds,
          bounds_scale,
          stepping_scale);

    optim.find_best();

    vector<float> in_args = optim.best_arg;
    const opt_val_wrap& best_res = optim.best_result;

    cout.clear();
    cout << (simple_output ? "" : "\nBest:\n") << (simple_output ? best_res.data->print_very_simple() : best_res.data->print_full()) << endl;
    if (silent) {
        cout.setstate(ios::failbit);
    }
    return 0;
}

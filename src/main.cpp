#include <chrono>
#include <cmath>
#include <format>
#include <functional>
#include <iostream>
#include <limits>
#include <numeric>
#include <string>
#include <vector>

#include <argparse/args.hpp>
#include <argparse/read.hpp>

#include "constants.hpp"
#include "optimiser.hpp"
#include "gas.hpp"
#include "tank.hpp"

using namespace std;

string list_gases() {
    string out;
    for (const gas_type& g : gases) {
        out += g.name + ", ";
    }
    out.resize(out.size() - 2);
    return out;
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

float get_gasmix_spec_heat(const vector<gas_ref>& gases, const vector<float>& ratios) {
    float total_heat_cap = 0;
    float total_ratio = 0;
    for(size_t i = 0; i < gases.size(); ++i) {
        total_heat_cap += gases[i].specific_heat() * ratios[i];
        total_ratio += ratios[i];
    }
    if (total_ratio == 0) return 0;
    return total_heat_cap / total_ratio;
}

float mix_input_setup(gas_tank& tank, const vector<gas_ref>& mix_gases, const vector<float>& mix_ratios, const vector<gas_ref>& primer_gases, const vector<float>& primer_ratios, float fuel_temp, float primer_temp, float target_temp) {
    float fuel_specheat = get_gasmix_spec_heat(mix_gases, mix_ratios);
    float primer_specheat = get_gasmix_spec_heat(primer_gases, primer_ratios);
    if (primer_specheat == 0) return -1.0;
    float fuel_pressure = (target_temp / primer_temp - 1.0) * pressure_cap / (fuel_specheat / primer_specheat - 1.0 + target_temp * (1.0 / primer_temp - fuel_specheat / primer_specheat / fuel_temp));

    float fuel_mols = to_mols(fuel_pressure, tank.mix.volume, fuel_temp);
    float total_mix_ratio = 0;
    for(float r : mix_ratios) total_mix_ratio += r;
    if(total_mix_ratio > 0) {
        for(size_t i = 0; i < mix_gases.size(); ++i) {
            tank.mix.set_amount_of(mix_gases[i], fuel_mols * mix_ratios[i] / total_mix_ratio);
        }
    }

    float primer_mols = to_mols(pressure_cap - fuel_pressure, tank.mix.volume, primer_temp);
    float total_primer_ratio = 0;
    for(float r : primer_ratios) total_primer_ratio += r;
    if(total_primer_ratio > 0) {
        for(size_t i = 0; i < primer_gases.size(); ++i) {
            tank.mix.set_amount_of(primer_gases[i], primer_mols * primer_ratios[i] / total_primer_ratio);
        }
    }

    tank.mix.temperature = (fuel_mols * fuel_specheat * fuel_temp + primer_mols * primer_specheat * primer_temp) / (fuel_mols * fuel_specheat + primer_mols * primer_specheat);
    return fuel_pressure;
}

struct bomb_data {
    vector<float> mix_ratios, primer_ratios;
    float fuel_temp, fuel_pressure, thir_temp, mix_to_pressure, mix_to_temp;
    vector<gas_ref> mix_gases, primer_gases;
    gas_tank tank;
    float radius = 0.0, fin_temp = -1.0, fin_pressure = -1.0, fin_heat_leak = -1.0, optstat = -1.0;
    int ticks = -1;

    bomb_data(vector<float> mix_ratios, vector<float> primer_ratios, float fuel_temp, float fuel_pressure, float thir_temp, float mix_to_pressure, float mix_to_temp, const vector<gas_ref>& mix_gases, const vector<gas_ref>& primer_gases):
        mix_ratios(mix_ratios), primer_ratios(primer_ratios), fuel_temp(fuel_temp), fuel_pressure(fuel_pressure), thir_temp(thir_temp), mix_to_pressure(mix_to_pressure), mix_to_temp(mix_to_temp), mix_gases(mix_gases), primer_gases(primer_gases) {};

    void results(float n_radius, float n_fin_temp, float n_fin_pressure, float n_optstat, int n_ticks, gas_tank n_tank) {
        radius = n_radius;
        fin_temp = n_fin_temp;
        fin_pressure = n_fin_pressure;
        optstat = n_optstat;
        ticks = n_ticks;
        tank = n_tank;
    }

    string print_very_simple() const {
        // optstat guaranteed to come first in the format `os=<number> `
        string out = "os=" + to_string(optstat) + " ti=" + to_string(ticks);
        out += " ft=" + to_string(fuel_temp) + " fp=" + to_string(fuel_pressure);
        out += " mp=" + to_string(mix_to_pressure) + " mt=" + to_string(mix_to_temp);
        out += " tt=" + to_string(thir_temp);
        out += " mi=";
        float total_mix_ratio = 0;
        for(float r : mix_ratios) total_mix_ratio += r;
        for(size_t i = 0; i < mix_gases.size(); ++i) {
            out += (string)mix_gases[i].name() + ":" + to_string(mix_ratios[i] / total_mix_ratio) + (i == mix_gases.size() - 1 ? "" : ",");
        }
        out += " pm=";
        float total_primer_ratio = 0;
        for(float r : primer_ratios) total_primer_ratio += r;
        for(size_t i = 0; i < primer_gases.size(); ++i) {
            out += (string)primer_gases[i].name() + ":" + to_string(primer_ratios[i] / total_primer_ratio) + (i == primer_gases.size() - 1 ? "" : ",");
        }
        return out;
    }

    string print_inline() const {
        string out_str;

        size_t mix_c = mix_gases.size(), primer_c = primer_gases.size();

        // get mix and primer fractions for each gas
        vector<float> mix_fractions(mix_c);
        float total_mix_ratio = accumulate(mix_ratios.begin(), mix_ratios.end(), 0.f);
        for (size_t i = 0; i < mix_c; ++i) {
            mix_fractions[i] = mix_ratios[i] / total_mix_ratio;
        }

        vector<float> primer_fractions(primer_c);
        float total_primer_ratio = accumulate(primer_ratios.begin(), primer_ratios.end(), 0.f);
        for (size_t i = 0; i < primer_c; ++i) {
            primer_fractions[i] = primer_ratios[i] / total_primer_ratio;
        }

        float required_primer_p = pressure_cap + (pressure_cap - fuel_pressure);

        out_str += format("[ time {:.1f}s | radius {:.2f}til | optstat {} ]; ", ticks * tickrate, radius, optstat);

        out_str += "M: [ ";
        for (size_t i = 0; i < mix_c; ++i) {
            out_str += format("{}% {} | ", mix_fractions[i] * 100.f, mix_gases[i].name());
        }
        out_str += format("{}K | {}kPa ]; ", fuel_temp, fuel_pressure);

        out_str += "C: [ ";
        for (size_t i = 0; i < primer_c; ++i) {
            out_str += format("{}% {} | ", primer_fractions[i] * 100.f, primer_gases[i].name());
        }
        out_str += format("{}K | >{}kPa ]", thir_temp, required_primer_p);

        return out_str;
    }

    string print_full() const {
        string out_str;

        size_t mix_c = mix_gases.size(), primer_c = primer_gases.size(), total_c = mix_c + primer_c;

        // get mix and primer fractions for each gas
        vector<float> mix_fractions(mix_c);
        float total_mix_ratio = accumulate(mix_ratios.begin(), mix_ratios.end(), 0.f);
        for (size_t i = 0; i < mix_c; ++i) {
            mix_fractions[i] = mix_ratios[i] / total_mix_ratio;
        }

        vector<float> primer_fractions(primer_c);
        float total_primer_ratio = accumulate(primer_ratios.begin(), primer_ratios.end(), 0.f);
        for (size_t i = 0; i < primer_c; ++i) {
            primer_fractions[i] = primer_ratios[i] / total_primer_ratio;
        }

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

        out_str += format("STATS: [ time {:.1f}s | radius {:.2f}til | optstat {} ]\n", ticks * tickrate, radius, optstat);

        out_str += "MIX:   [ ";
        for (size_t i = 0; i < mix_c; ++i) {
            out_str += format("{}% {} | ", mix_fractions[i] * 100.f, mix_gases[i].name());
        }
        out_str += format("{}K | {}kPa ]\n", fuel_temp, fuel_pressure);

        out_str += "CAN:   [ ";
        for (size_t i = 0; i < primer_c; ++i) {
            out_str += format("{}% {} | ", primer_fractions[i] * 100.f, primer_gases[i].name());
        }
        out_str += format("{}K | >{}kPa ]\n", thir_temp, required_primer_p);

        out_str += "REQ:   [ ";
        for (size_t i = 0; i < total_c; ++i) {
            out_str += format("{:.2f}mol {}", min_amounts[i].first, min_amounts[i].second);
            if (i + 1 != total_c) out_str += " | ";
        }
        out_str += " ]";

        return out_str;
    }
};

void print_bomb(const bomb_data& bomb, const string& what, bool extensive = false, bool simple = false) {
    cout << what << (simple ? bomb.print_very_simple() : (extensive ? bomb.print_full() : bomb.print_inline())) << endl;
}

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
opt_val_wrap do_sim(const vector<float>& in_args, tuple<const vector<gas_ref>&, const vector<gas_ref>&, bool, size_t> args) {
    float target_temp = in_args[0];
    float fuel_temp = in_args[1];
    float thir_temp = in_args[2];
    const vector<gas_ref>& mix_gases = get<0>(args);
    const vector<gas_ref>& primer_gases = get<1>(args);
    bool measure_before = get<2>(args);
    size_t tick_cap = get<3>(args);

    float fuel_pressure;
    gas_tank tank;
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
    fuel_pressure = mix_input_setup(tank, mix_gases, mix_ratios, primer_gases, primer_ratios, fuel_temp, thir_temp, target_temp);
    if (fuel_pressure > pressure_cap || fuel_pressure < 0.0) {
        return {0.f, 0.f, false};
    }
    tank.tick_n(tick_cap);
    // TODO critical
    /*
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
    } */
    return {tank.radius_cache, tank.mix.pressure(), true};
}

// args: target_temp, fuel_temp, thir_temp, mix ratios..., primer ratios...
bomb_data get_data(const vector<float>& in_args, tuple<const vector<gas_ref>&, const vector<gas_ref>&, bool, size_t> args) {
    float target_temp = in_args[0];
    float fuel_temp = in_args[1];
    float thir_temp = in_args[2];
    const vector<gas_ref>& mix_gases = get<0>(args);
    const vector<gas_ref>& primer_gases = get<1>(args);
    bool measure_before = get<2>(args);
    size_t tick_cap = get<3>(args);

    float fuel_pressure;
    gas_tank tank;
    vector<float> mix_ratios(mix_gases.size(), 1.f);
    for (size_t i = 0; i < mix_gases.size() - 1; ++i) {
        mix_ratios[i + 1] = in_args[3 + i];
    }
    vector<float> primer_ratios(primer_gases.size(), 1.f);
    size_t mg_s = mix_gases.size() - 1;
    for (size_t i = 0; i < primer_gases.size() - 1; ++i) {
        primer_ratios[i + 1] = in_args[3 + mg_s + i];
    }
    fuel_pressure = mix_input_setup(tank, mix_gases, mix_ratios, primer_gases, primer_ratios, fuel_temp, thir_temp, target_temp);
    float mix_pressure = tank.mix.pressure();
    size_t tick = tank.tick_n(tick_cap);
    /*if (!measure_before) {
        stat = optimise_stat();
    }*/
    bomb_data bomb(mix_ratios, primer_ratios, fuel_temp, fuel_pressure, thir_temp, mix_pressure, target_temp, mix_gases, primer_gases);
    bomb.results(tank.radius_cache, tank.mix.temperature, tank.mix.pressure(), tank.radius_cache, tick, tank);
    return bomb;
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

    optimizer<tuple<const vector<gas_ref>&, const vector<gas_ref>&, bool, size_t>, opt_val_wrap>
    optim(do_sim,
          lower_bounds,
          upper_bounds,
          min_l_step,
          min_e_step,
          optimise_maximise,
          make_tuple(ref(mix_gases), ref(primer_gases), optimise_measure_before, tick_cap),
          chrono::duration<float>(max_runtime),
          sample_rounds,
          bounds_scale,
          stepping_scale);

    optim.find_best();

    vector<float> in_args = optim.best_arg;
    bomb_data best_bomb = get_data(in_args, make_tuple(ref(mix_gases), ref(primer_gases), optimise_measure_before, tick_cap));

    cout.clear();
    cout << (simple_output ? "" : "\nBest:\n") << (simple_output ? best_bomb.print_very_simple() : best_bomb.print_full()) << endl;
    if (silent) {
        cout.setstate(ios::failbit);
    }
    return 0;
}

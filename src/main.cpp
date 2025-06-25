#include <chrono>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include <argparse/args.hpp>
#include <argparse/read.hpp>

#include "constants.hpp"
#include "optimiser.hpp"
#include "gas.hpp"
#include "sim.hpp"

using namespace std;
using namespace asim;

template<typename T>
T get_input() {
    while (true) {
        while (cin.peek() == '\n') cin.get();
        T val;
        cin >> val;
        bool bad = !cin;
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        if (bad) cout << "Invalid input. Try again: ";
        else return val;
    }
}

template<typename T>
T input_or_default(const T& default_value) {
    if (cin.get() == '\n') return default_value;
    cin.unget();
    T val = get_input<T>();
    return val;
}

template<typename T>
bool try_input(T& into) {
    if (cin.get() == '\n') return false;
    cin.unget();
    into = get_input<T>();
    return true;
}

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

EM_JS(void, setup_console, (), {
    Module.print = function(text) {
        console.log(text);
        if (typeof window.updateOutput === 'function') {
            window.updateOutput(text);
        }
    };
});

int main(int argc, char* argv[]) {
    setup_console();
#else
int main(int argc, char* argv[]) {
#endif
    size_t log_level = 2;

    enum _mode {normal, mixing, full_input};
    _mode mode = normal;

    bool mixing_mode = false, full_input_mode = false;
    bool simple_output = false, silent = false;

    vector<gas_ref> mix_gases;
    vector<gas_ref> primer_gases;
    float mixt1 = 0.f, mixt2 = 0.f, thirt1 = 0.f, thirt2 = 0.f;
    float ratio_bounds = 10.f;
    float ratio_step = 1.005f;
    float temperature_step = 1.001f, temperature_step_min = 0.05f;
    float lower_target_temp = fire_temp + 0.1f;
    float lower_pressure = pressure_cap;
    bool step_target_temp = false;
    size_t tick_cap = 10 * 60 / tickrate; // 10 minutes

    tuple<field_ref<bomb_data>, bool, bool> opt_params{bomb_data::radius_field, true, false};

    vector<field_restriction<bomb_data>> pre_restrictions;
    vector<field_restriction<bomb_data>> post_restrictions;

    float max_runtime = 3.f;
    size_t sample_rounds = 5;
    float bounds_scale = 0.5f;
    float stepping_scale = 0.75f;
    size_t nthreads = 1;

    std::vector<std::shared_ptr<argp::base_argument>> args = {
        argp::make_argument("ratiobounds", "", "set gas ratio iteration bounds", ratio_bounds),
        argp::make_argument("mixtoiter", "s", "provide potentially better results by also iterating the mix-to temperature (WARNING: will take many times longer to calculate)", step_target_temp),
        argp::make_argument("mixingmode", "m", "UTILITY TOOL: utility to find desired mixer percentage if mixing different-temperature gases", mixing_mode),
        argp::make_argument("fullinput", "f", "UTILITY TOOL: simulate and print every tick of a bomb with chosen gases", full_input_mode),
        argp::make_argument("mixg", "mg", "list of fuel gases (usually, in tank)", mix_gases),
        argp::make_argument("primerg", "pg", "list of primer gases (usually, in canister)", primer_gases),
        argp::make_argument("mixt1", "m1", "minimum fuel mix temperature to check, Kelvin", mixt1),
        argp::make_argument("mixt2", "m2", "maximum fuel mix temperature to check, Kelvin", mixt2),
        argp::make_argument("thirt1", "t1", "minimum primer mix temperature to check, Kelvin", thirt1),
        argp::make_argument("thirt2", "t2", "maximum primer mix temperature to check, Kelvin", thirt2),
        argp::make_argument("lowerp", "p1", "lower mix-to pressure to check, kPa, default is pressure cap", lower_pressure),
        argp::make_argument("ticks", "t", "set tick limit: aborts if a bomb takes longer than this to detonate (default: " + to_string(tick_cap) + ")", tick_cap),
        argp::make_argument("tstep", "", "set temperature iteration multiplier (default " + to_string(temperature_step) + ")", temperature_step),
        argp::make_argument("tstepm", "", "set minimum temperature iteration step (default " + to_string(temperature_step_min) + ")", temperature_step_min),
        argp::make_argument("lowertargettemp", "o", "only consider bombs which mix to above this temperature; higher values may make bombs more robust to slight mismixing (default " + to_string(lower_target_temp) + ")", lower_target_temp),
        argp::make_argument("loglevel", "l", "how much to log (default " + to_string(log_level) + ")", log_level),
        argp::make_argument("param", "p", "(param, maximise, measure_before_sim): lets you configure what parameter and how to optimise", opt_params),
        argp::make_argument("restrictpre", "rb", "lets you make atmosim not consider bombs outside of chosen parameters, measured before simulation", pre_restrictions),
        argp::make_argument("restrictpost", "ra", "same as -rr, but measured after simulation", post_restrictions),
        argp::make_argument("simpleout", "", "makes very simple output, for use by other programs or advanced users", simple_output),
        argp::make_argument("silent", "", "output ONLY the final result, overrides loglevel", silent),
        argp::make_argument("runtime", "rt", "for how long to run in seconds (default " + to_string(max_runtime) + ")", max_runtime),
        argp::make_argument("samplerounds", "sr", "how many sampling rounds to perform, multiplies runtime (default " + to_string(sample_rounds) + ")", sample_rounds),
        argp::make_argument("boundsscale", "", "how much to scale bounds each sample round (default " + to_string(bounds_scale) + ")", bounds_scale),
        argp::make_argument("steppingscale", "", "how much to scale minimum step each sample round (default " + to_string(stepping_scale) + ")", stepping_scale),
        argp::make_argument("nthreads", "j", "number of threads for the optimiser to use", nthreads)
    };

    argp::parse_arguments(args, argc, argv,
    // pre-help
        "Atmosim: SS14 atmos maxcap calculator utility\n"
        "  This program contains an optimisation algorithm that attempts to find the best bomb possible according to the desired parameters.\n"
        "  Additionally, there's a few extra utility tools you can activate instead of the primary mode with their respective flags.\n"
        "\n"
        "  Available parameter types:\n"
        "    " + params_supported_str +
        "\n"
        "  Available gas types:\n"
        "    " + list_gases() +
        "\n",
    // post-help
        "\n"
        "Example usage:\n"
        "  $ ./atmosim -mg=[plasma,tritium] -pg=[oxygen] -m1=375.15 -m2=595.15 -t1=293.15 -t2=293.15 -rt=0.5 -sr=10\n"
        "  This should find you a ~13.5 radius maxcap recipe. Experiment with other parameters.\n"
        "  For --restrictpre (-rb) and --restrictpost (-ra):\n"
        "  $ ./atmosim -mg=[plasma,tritium] -pg=[oxygen] -m1=375.15 -m2=595.15 -t1=293.15 -t2=293.15 -ra=[[radius,0,11],[ticks,20,44]]\n"
        "  The -ra and -rb arguments will interpret `-` as infinity in the respective direction, and the second argument may be omitted.\n"
        "  -ra=[[radius,20]] or -ra=[[radius,20,-]] will restrict to any radius above 20, and -ra=[[radius,-,15]] will restrict to radius below 15.\n"
        "  $ ./atmosim -mg=[nitrous_oxide,tritium] -pg=[oxygen,frezon] -m1=73.15 -m2=293.15 -t1=373.15 -t2=800.15 -ra=[[radius,20]] --ticks=1200 -rt=5 -sr=8 -p=[ticks,true,false]\n"
        "\n"
        "Tips and tricks\n"
        "  Consider using the -s flag for radius-optimised bombs. Not recommended for ticks-optimised bombs.\n"
        "  Additionally, consider letting the optimiser think for longer using the -rt and -sr flags.\n"
        "  If you want a long-fuse bomb, try using the -p flag to optimise to maximise ticks and the -ra flag to restrict radius to be above a desired value.\n"
        "  Remember to use the -t flag to raise maximum alotted ticks if you're trying to find long-fuse bombs.\n"
        "\n"
        "  Brought to you by Ilya246 and friends"
    );

    // check if we chose an alternate mode
    if (mixing_mode) mode = mixing;
    if (full_input_mode) mode = full_input;

    switch (mode) {
        case (mixing): {
            cout << "Input desired % of first gas: ";
            float perc = get_input<float>();
            cout << "Input temperature of first gas: ";
            float T1 = get_input<float>();
            cout << "Input temperature of second gas: ";
            float T2 = get_input<float>();
            float portion = perc * 0.01f;
            float n_ratio = portion / (1.f - portion) * T1 / T2;
            float n_perc = 100.f * n_ratio / (1.f + n_ratio);
            cout << format("Desired percentage: {}% first {}% second", n_perc, 100.f - n_perc) << endl;
            break;
        }
        case (full_input): {
            gas_tank tank;

            cout << "Input number of mixes (omit for 2): ";
            int mix_c = input_or_default(2);
            for (int i = 0; i < mix_c; ++i) {
                cout << format("Inputting mix {}\n", i + 1);
                cout << format("Input pressure to fill to (omit for {}): ", pressure_cap);
                float pressure_to = input_or_default(pressure_cap);
                cout << "Input temperature: ";
                float temperature = get_input<float>();
                vector<pair<gas_ref, float>> gases;
                while (true) {
                    gas_ref g;
                    cout << "Input gas (omit to end): ";
                    if (!try_input(g)) break;
                    cout << "Input ratio (%, portion): ";
                    float ratio = get_input<float>();
                    gases.push_back({g, ratio});
                }
                tank.mix.canister_fill_to(get_fractions(gases), temperature, pressure_to);
            }

            size_t tick = 1;
            do {
                cout << format("[Tick {:<2}] Tank status: {}", tick, tank.get_status());
                cout << endl;
                ++tick;
            } while (tank.tick());

            cout << format("Result: status: {}, state {}, range {}", tank.get_status(), (int)tank.state, tank.calc_radius());
            cout << endl;
            break;
        }
        default: {
            break;
        }
    }

    if (mode != normal) return 0;

    field_ref<bomb_data> opt_param = get<0>(opt_params);
    bool optimise_maximise = get<1>(opt_params);
    bool optimise_measure_before = get<2>(opt_params);

    if (silent) {
        // stop talking, be quiet for several days
        cout.setstate(ios::failbit);
    }

    if ((mix_gases.empty() || primer_gases.empty()) && !silent) {
        cout << "No mix or primer gases found, see `./atmosim -h` for usage\n";
        cout << "Gases: " << list_gases() << endl;
        return 0;
    }

    size_t num_temps = 3;
    size_t pressure_offset = num_temps;
    size_t num_pressure = 1;
    size_t ratio_offset = num_temps + num_pressure;
    size_t num_mix_ratios = mix_gases.size() > 1 ? mix_gases.size() - 1 : 0;
    size_t num_primer_ratios = primer_gases.size() > 1 ? primer_gases.size() - 1 : 0;
    size_t num_params = ratio_offset + num_mix_ratios + num_primer_ratios;

    vector<float> lower_bounds = {std::min(mixt1, thirt1), mixt1, thirt1, lower_pressure};
    lower_bounds[0] = std::max(lower_target_temp, lower_bounds[0]);
    vector<float> upper_bounds = {std::max(mixt2, thirt2), mixt2, thirt2, pressure_cap};
    if (!step_target_temp) {
        upper_bounds[0] = lower_bounds[0];
    }
    for (size_t i = 0; i < num_params - ratio_offset; ++i) {
        lower_bounds.push_back(1.f / ratio_bounds);
        upper_bounds.push_back(ratio_bounds);
    }

    vector<float> min_l_step(lower_bounds.size(), 0.f);
    vector<float> min_e_step(lower_bounds.size(), 0.f);
    for (size_t i = 0; i < num_temps; ++i) {
        min_l_step[i] = temperature_step_min;
        min_e_step[i] = temperature_step;
    }
    min_l_step[pressure_offset] = pressure_cap * 0.01f;
    min_e_step[pressure_offset] = 1.01f;
    for (size_t i = ratio_offset; i < num_params; ++i) {
        min_l_step[i] = 0.f;
        min_e_step[i] = ratio_step;
    }

    optimiser<tuple<vector<gas_ref>, vector<gas_ref>, bool, size_t, field_ref<bomb_data>, vector<field_restriction<bomb_data>>, vector<field_restriction<bomb_data>>>, opt_val_wrap>
    optim(do_sim,
          lower_bounds,
          upper_bounds,
          min_l_step,
          min_e_step,
          optimise_maximise,
          make_tuple(mix_gases, primer_gases, optimise_measure_before, tick_cap, opt_param, pre_restrictions, post_restrictions),
          chrono::duration<float>(max_runtime),
          sample_rounds,
          bounds_scale,
          stepping_scale,
          log_level);
    optim.thread_count = nthreads;

    optim.find_best();

    vector<float> in_args = optim.best_arg;
    const opt_val_wrap& best_res = optim.best_result;

    cout.clear();
    cout << (simple_output ? "" : "\nBest:\n") << (simple_output ? best_res.data->print_very_simple() : best_res.data->print_full()) << endl;
    if (silent) {
        cout.setstate(ios::failbit);
    }
    int ret = 0;
#ifdef __EMSCRIPTEN__
    // Free allocated arguments
    for(int i = 0; i < argc; i++) {
        free(argv[i]);
    }
    free(argv);
#endif
    return ret;
}

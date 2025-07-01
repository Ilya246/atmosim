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
#include "utility.hpp"

using namespace std;
using namespace asim;

template<typename T>
T get_input() {
    while (true) {
        while (cin.peek() == '\n') cin.get();
        std::string val_str;
        getline(cin, val_str);
        if (val_str.back() == '\n') val_str.pop_back();
        if(!cin) {
            if (status_SIGINT) throw runtime_error("Got SIGINT");
            cin.clear();
            cout << "Invalid input. Try again: ";
        }
        try {
            T val = argp::parse_value<T>(val_str);
            return val;
        } catch (const argp::read_error& e) {
            cout << "Invalid input. Try again: ";
        }
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
    handle_sigint();

    size_t log_level = 2;

    enum struct work_mode {normal, mixing, full_input, tolerances};
    work_mode mode = work_mode::normal;

    bool mixing_mode = false, full_input_mode = false, tolerances_mode = false;
    bool simple_output = false, silent = false;

    vector<gas_ref> mix_gases;
    vector<gas_ref> primer_gases;
    float mixt1 = 0.f, mixt2 = 0.f, thirt1 = 0.f, thirt2 = 0.f;
    float ratio_bound = 3.f;
    tuple<vector<float>, vector<float>> ratio_bounds;
    float lower_target_temp = fire_temp + 0.1f;
    float lower_pressure = pressure_cap, upper_pressure = pressure_cap;
    bool step_target_temp = false;
    size_t tick_cap = numeric_limits<size_t>::max(); // 10 minutes
    bool do_round = true;
    // note: this is percentage
    float round_ratio_to = 0.001f; // default is 0.001% to mitigate FP inaccuracy

    tuple<field_ref<bomb_data>, bool, bool> opt_params{bomb_data::radius_field, true, false};

    vector<field_restriction<bomb_data>> pre_restrictions;
    vector<field_restriction<bomb_data>> post_restrictions;

    float max_runtime = 3.f;
    size_t sample_rounds = 5;
    float bounds_scale = 0.5f;
    size_t nthreads = 1;

    std::vector<std::shared_ptr<argp::base_argument>> args = {
        argp::make_argument("ratiob", "", "set gas ratio iteration bound", ratio_bound),
        argp::make_argument("ratiobounds", "rbs", "set gas ratio iteration bounds: exact setup", ratio_bounds),
        argp::make_argument("mixtoiter", "s", "provide potentially better results by also iterating the mix-to temperature (WARNING: will take many times longer to calculate)", step_target_temp),
        argp::make_argument("mixingmode", "m", "UTILITY TOOL: utility to find desired mixer percentage if mixing different-temperature gases", mixing_mode),
        argp::make_argument("fullinput", "f", "UTILITY TOOL: simulate and print every tick of a bomb with chosen gases", full_input_mode),
        argp::make_argument("tolerance", "", "UTILITY TOOL: measure tolerances for a bomb serialised string", tolerances_mode),
        argp::make_argument("mixg", "mg", "list of fuel gases (usually, in tank)", mix_gases),
        argp::make_argument("primerg", "pg", "list of primer gases (usually, in canister)", primer_gases),
        argp::make_argument("mixt1", "m1", "minimum fuel mix temperature to check, Kelvin", mixt1),
        argp::make_argument("mixt2", "m2", "maximum fuel mix temperature to check, Kelvin", mixt2),
        argp::make_argument("thirt1", "t1", "minimum primer mix temperature to check, Kelvin", thirt1),
        argp::make_argument("thirt2", "t2", "maximum primer mix temperature to check, Kelvin", thirt2),
        argp::make_argument("round", "r", "whether to round pressures and temperatures to settable values", do_round),
        argp::make_argument("roundratio", "", "also round ratio to this much", round_ratio_to),
        argp::make_argument("lowerp", "p1", "lower mix-to pressure to check, kPa, default is pressure cap", lower_pressure),
        argp::make_argument("upperp", "p2", "upper mix-to pressure to check, kPa, default is pressure cap", upper_pressure),
        argp::make_argument("ticks", "t", "set tick limit: aborts if a bomb takes longer than this to detonate (default: " + to_string(tick_cap) + ")", tick_cap),
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
    if (mixing_mode) mode = work_mode::mixing;
    if (full_input_mode) mode = work_mode::full_input;
    if (tolerances_mode) mode = work_mode::tolerances;

    switch (mode) {
        case (work_mode::mixing): {
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
        case (work_mode::full_input): {
            gas_tank tank;

            cout << "Normal (y) or serialized (n) input [Y/n]: ";
            bool norm_input;
            if (!try_input<bool>(norm_input)) {
                norm_input = true;
            }
            if (!norm_input) {
                cout << "Input serialised string: ";
                std::string str;
                getline(cin, str);
                bomb_data data = bomb_data::deserialize(str);
                tank = data.tank;
            } else {
                cout << "Input number of mixes (omit for 2): ";
                int mix_c = input_or_default(2);
                for (int i = 0; i < mix_c; ++i) {
                    cout << format("Inputting mix {}\n", i + 1);
                    cout << format("Input pressure to fill to (omit for {}): ", pressure_cap);
                    float pressure_to = input_or_default(pressure_cap);
                    cout << "Input temperature: ";
                    float temperature = get_input<float>();
                    vector<pair<gas_ref, float>> gases;
                    float ratio_sum = 0.f;
                    bool end = false;
                    while (!end) {
                        gas_ref g;
                        cout << format("Input gas (omit to end): ", list_gases());
                        if (!try_input(g)) break;
                        cout << "Input ratio (%, portion; omit for remainder from 100%): ";
                        float ratio;
                        if (!try_input(ratio)) {
                            ratio = 100.f - ratio_sum;
                            end = true;
                        }
                        ratio_sum += ratio;
                        gases.push_back({g, ratio});
                    }
                    tank.mix.canister_fill_to(get_fractions(gases), temperature, pressure_to);
                }
            }

            size_t tick = 1;
            while (true) {
                cout << format("[Tick {:<2}] Tank status: {}", tick, tank.get_status()) << endl;
                if (!tank.tick() || tank.state != tank.st_intact || status_SIGINT)
                    break;
                ++tick;
            }

            const char* state_name = "unknown";
            switch (tank.state) {
                case gas_tank::st_intact: state_name = "intact"; break;
                case gas_tank::st_ruptured: state_name = "ruptured"; break;
                case gas_tank::st_exploded: state_name = "exploded"; break;
            }
            cout << format("Result:\n  Status: {}\n  State: {}\n  Radius: {:.2f}",
                            tank.get_status(), state_name, tank.calc_radius()) << endl;
            break;
        }
        case (work_mode::tolerances): {
            cout << "Input serialised string: ";
            std::string str;
            getline(cin, str);
            bomb_data data = bomb_data::deserialize(str);
            data.ticks = data.tank.tick_n(tick_cap);
            data.fin_radius = data.tank.calc_radius();
            data.fin_pressure = data.tank.mix.pressure();
            cout << "Input desired tolerance (omit for 0.95): ";
            float tol = input_or_default<float>(0.95f);
            cout << "Tolerances:\n" << data.measure_tolerances(tol) << endl;
            break;
        }
        default: {
            break;
        }
    }

    if (mode != work_mode::normal) return 0;

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

    size_t num_mix_ratios = mix_gases.size() > 1 ? mix_gases.size() - 1 : 0;
    size_t num_primer_ratios = primer_gases.size() > 1 ? primer_gases.size() - 1 : 0;
    size_t num_ratios = num_mix_ratios + num_primer_ratios;

    vector<float> lower_bounds = {std::min(mixt1, thirt1), mixt1, thirt1, lower_pressure};
    lower_bounds[0] = std::max(lower_target_temp, lower_bounds[0]);
    vector<float> upper_bounds = {std::max(mixt2, thirt2), mixt2, thirt2, upper_pressure};
    if (!step_target_temp) {
        upper_bounds[0] = lower_bounds[0];
    }

    vector<float> ratio_b_low = get<0>(ratio_bounds);
    vector<float> ratio_b_high = get<1>(ratio_bounds);
    if (!ratio_b_low.empty() || !ratio_b_high.empty()) {
        if (ratio_b_low.size() != ratio_b_high.size() || ratio_b_low.size() != num_ratios) {
            cout << "Invalid number of custom ratio bounds provided. Provide ratio bounds for the last count - 1 gases in each mix." << endl;
            return 1;
        }
        for (size_t i = 0; i < num_ratios; ++i) {
            lower_bounds.push_back(ratio_b_low[i]);
            upper_bounds.push_back(ratio_b_high[i]);
        }
    } else {
        for (size_t i = 0; i < num_ratios; ++i) {
            lower_bounds.push_back(-ratio_bound);
            upper_bounds.push_back(ratio_bound);
        }
    }

    optimiser<bomb_args, opt_val_wrap>
    optim(do_sim,
          lower_bounds,
          upper_bounds,
          optimise_maximise,
          {mix_gases, primer_gases, optimise_measure_before, do_round, round_ratio_to, tick_cap, opt_param, pre_restrictions, post_restrictions},
          as_seconds(max_runtime),
          sample_rounds,
          bounds_scale,
          log_level);
    optim.n_threads = nthreads;

    optim.find_best();

    const opt_val_wrap& best_res = optim.best_result;
    cout.clear();
    cout << (simple_output ? "" : "\nBest:\n") << (simple_output ? best_res.data->print_very_simple() : best_res.data->print_full()) << endl;
    if (!simple_output) {
        cout << "\nSerialized string: " << best_res.data->serialize() << endl;
    }
    cout << default_tol << "x tolerances:\n" << best_res.data->measure_tolerances() << endl;
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

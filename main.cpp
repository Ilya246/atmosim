#include "argparse/args.hpp"
#include "argparse/read.hpp"
#include "core.hpp"
#include "gas.hpp"
#include "tank.hpp"
#include "sim.hpp"
#include "optim.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <tuple>
#include <memory>

using namespace std;

dyn_val optimise_val = {float_val, &radius};
vector<shared_ptr<base_restriction>> pre_restrictions;
vector<shared_ptr<base_restriction>> post_restrictions;

// Parameter map for sim_params
unordered_map<string, dyn_val> sim_params{
    {"radius",      {float_val, &radius     }},
    {"temperature", {float_val, &temperature}},
    {"leaked_heat",  {float_val, &leaked_heat }},
    {"ticks",       {int_val,   &tick       }},
    {"tank_state",   {int_val,   &cur_state  }}
};

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

int main(int argc, char* argv[]) {
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
        cout.setstate(ios::failbit);
    }
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
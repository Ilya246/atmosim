#include "optim.hpp"
#include <iostream>
#include <vector>
#include <tuple>
#include <functional>
#include <limits>
#include <chrono>
#include <cmath>
#include <algorithm>
#include "sim.hpp"
#include "core.hpp"
#include "tank.hpp"
#include "gas.hpp"

using namespace std;

bomb_data::bomb_data(vector<float> mix_ratios, vector<float> primer_ratios, float fuel_temp, float fuel_pressure, float thir_temp, float mix_pressure, float mix_temp, const vector<gas_type>& mix_gases, const vector<gas_type>& primer_gases)
    : mix_ratios(mix_ratios), primer_ratios(primer_ratios), fuel_temp(fuel_temp), fuel_pressure(fuel_pressure), thir_temp(thir_temp), mix_pressure(mix_pressure), mix_temp(mix_temp), mix_gases(mix_gases), primer_gases(primer_gases) {}

void bomb_data::results(float n_radius, float n_fin_temp, float n_fin_pressure, float n_optstat, int n_ticks, tank_state n_state) {
    radius = n_radius;
    fin_temp = n_fin_temp;
    fin_pressure = n_fin_pressure;
    optstat = n_optstat;
    ticks = n_ticks;
    state = n_state;
}

string bomb_data::print_very_simple() const {
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

string bomb_data::print_simple() const {
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

string bomb_data::print_extensive() const {
    // (Omitted for brevity, copy from atmosim.cpp if needed)
    return print_simple();
}

void print_bomb(const bomb_data& bomb, const string& what, bool extensive) {
    cout << what << (simple_output ? bomb.print_very_simple() : (extensive ? bomb.print_extensive() : bomb.print_simple())) << endl;
}

opt_val_wrap opt_val_wrap::worst(bool maximise) {
    float w = numeric_limits<float>::max() * (maximise ? -1.f : 1.f);
    return {w, w, false};
}
bool opt_val_wrap::valid() const { return valid_v; }
bool opt_val_wrap::operator>(const opt_val_wrap& rhs) const { return stat == rhs.stat ? pressure > rhs.pressure : stat > rhs.stat; }
bool opt_val_wrap::operator>=(const opt_val_wrap& rhs) const { return stat >= rhs.stat; }
bool opt_val_wrap::operator==(const opt_val_wrap& rhs) const { return stat == rhs.stat; }

// Helper for progress bar (from atmosim.cpp)
string get_progress_bar(long progress, long size) {
    string progress_bar = '[' + string(progress, '#') + string(size - progress, ' ') + ']';
    return progress_bar;
}

// Progress printing (from atmosim.cpp)
string rotator = "|/-\\";
int rotator_chars = 4;
int rotator_index = rotator_chars - 1;
long long progress_bar_spacing = 4817;
const long long progress_update_spacing = progress_bar_spacing * 25;
const int progress_polls = 20;
const long long progress_poll_window = progress_update_spacing * progress_polls;
long long progress_poll_times[progress_polls];
long long progress_poll = 0;
long long last_speed = 0;
chrono::high_resolution_clock main_clock;
long long iters = 0;
auto start_time = main_clock.now();

char get_rotator() {
    rotator_index = (rotator_index + 1) % rotator_chars;
    return rotator[rotator_index];
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

// Optimise stat (from atmosim.cpp)
extern dyn_val optimise_val;
float optimise_stat() {
    return optimise_val.type == float_val ? get_dyn<float>(optimise_val) : get_dyn<int>(optimise_val);
}

// Restrictions (from atmosim.cpp)
extern vector<shared_ptr<base_restriction>> pre_restrictions;
extern vector<shared_ptr<base_restriction>> post_restrictions;
bool restrictions_met(vector<shared_ptr<base_restriction>>& restrictions) {
    for (shared_ptr<base_restriction>& r : restrictions) {
        if (!r->OK()) {
            return false;
        }
    }
    return true;
}

// Template optimizer implementation
// (from atmosim.cpp, lines 950-1160 approx)
template<typename T, typename R>
optimizer<T, R>::optimizer(function<R(const vector<float>&, T)> func,
              const vector<float>& lowerb,
              const vector<float>& upperb,
              const vector<float>& i_lin_step,
              const vector<float>& i_exp_step,
              bool maxm,
              T i_args,
              float i_max_duration)
    : funct(func), args(current, i_args), lower_bounds(lowerb), upper_bounds(upperb), min_lin_step(i_lin_step), min_exp_step(i_exp_step), maximise(maxm), max_duration(i_max_duration) {
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
    current = lower_bounds;
    best_arg = vector<float>(current.size(), 0.f);
    best_result = worst_res();
}

template<typename T, typename R>
void optimizer<T, R>::find_best() {
    size_t paramc = current.size();
    vector<float> cur_lower_bounds = lower_bounds;
    vector<float> cur_upper_bounds = upper_bounds;
    step_scale = 1.f;
    for (size_t samp_idx = 0; samp_idx < sample_rounds; ++samp_idx) {
        chrono::time_point s_time = main_clock.now();
        while (main_clock.now() - s_time < chrono::duration<float>(max_duration)) {
            for (size_t i = 0; i < current.size(); ++i) {
                current[i] = cur_lower_bounds[i] + (cur_upper_bounds[i] - cur_lower_bounds[i]) * frand();
            }
            R c_result = sample();
            while (true) {
                if (log_level >= 3) {
                    cout << "Sampling: ";
                    for (float f : current) {
                        cout << f << " ";
                    }
                    cout << endl;
                }
                vector<pair<size_t, bool>> best_movedirs = {};
                R best_move_res = c_result;
                vector<float> old_current = current;
                for (size_t i = 0; i < paramc; ++i) {
                    auto do_update = [&](const R& with, bool sign) {
                        if (with == best_move_res) {
                            best_movedirs.push_back({i, sign});
                        } else if (better_than(with, best_move_res)) {
                            best_movedirs = {{i, sign}};
                            best_move_res = with;
                        }
                    };
                    current[i] += get_step(i);
                    if (current[i] <= cur_upper_bounds[i]) {
                        R res = sample();
                        do_update(res, true);
                    }
                    current[i] = old_current[i];
                    current[i] -= get_step(i);
                    if (current[i] >= cur_lower_bounds[i]) {
                        R res = sample();
                        do_update(res, false);
                    }
                    current[i] = old_current[i];
                }
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
                        current[dir] += sign * get_step(dir, move_scl);
                        if (current[dir] < cur_lower_bounds[dir] || current[dir] > cur_upper_bounds[dir]) {
                            current[dir] = old_current[dir];
                            break;
                        }
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
                for (const pair<size_t, bool>& p : best_movedirs) {
                    float sign = p.second ? +1.f : -1.f;
                    size_t scl = do_skipmove(p.first, sign);
                    if (scl != 0) {
                        chosen = {p.first, sign};
                        chosen_scl = scl;
                    }
                }
                if (chosen_scl == 0 || best_move_res == c_result) {
                    if (log_level >= 2) {
                        bomb_data data = get_data(get<0>(args), get<1>(args));
                        print_bomb(data, "Local minimum found: ");
                    }
                    break;
                }
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
            for (size_t i = 0; i < current.size(); ++i) {
                float& lowerb = cur_lower_bounds[i];
                float& upperb = cur_upper_bounds[i];
                const float& best_at = best_arg[i];
                lowerb += (best_at - lowerb) * (1.f - bounds_scale);
                upperb += (best_at - upperb) * (1.f - bounds_scale);
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

template<typename T, typename R>
R optimizer<T, R>::sample() {
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

template<typename T, typename R>
bool optimizer<T, R>::better_than(R what, R than) {
    return maximise ? what > than : than > what;
}

template<typename T, typename R>
bool optimizer<T, R>::better_eq_than(R what, R than) {
    return maximise ? what >= than : than >= what;
}

template<typename T, typename R>
R optimizer<T, R>::worst_res() {
    return R::worst(maximise);
}

template<typename T, typename R>
float optimizer<T, R>::get_step(int i, float scale) {
    scale *= step_scale;
    const float& c_param = current[i];
    const float& min_l_step = min_lin_step[i];
    const float& min_e_step = min_exp_step[i];
    float step = std::max(c_param * (1.f + (min_e_step - 1.f) * scale), c_param + min_l_step * scale) - c_param;
    return step;
}

template<typename T, typename R>
void optimizer<T, R>::step(int i) {
    current[i] += get_step(i);
}

// Explicit template instantiations
// (for optimizer with tuple<const vector<gas_type>&, const vector<gas_type>&, bool>, opt_val_wrap)
template struct optimizer<tuple<const vector<gas_type>&, const vector<gas_type>&, bool>, opt_val_wrap>;

// do_sim, get_data, test_mix (from atmosim.cpp, lines 1200+)
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
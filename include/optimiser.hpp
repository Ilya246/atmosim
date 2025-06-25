#pragma once

#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <tuple>
#include <vector>

#include "utility.hpp"
#include "constants.hpp"

namespace asim {

template<typename T, typename R>
struct optimiser {
    std::function<R(const std::vector<float>&, T)> funct;
    std::tuple<const std::vector<float>&, T> args;
    std::vector<float> lower_bounds;
    std::vector<float> upper_bounds;
    std::vector<float> min_lin_step;
    std::vector<float> min_exp_step;
    bool maximise;
    std::chrono::duration<float> max_duration;

    size_t log_level;
    size_t sample_count = 0, last_sample_count = 0;
    size_t init_sample_count = 0, last_init_sample_count = 0;
    std::chrono::duration<float> log_spacing = std::chrono::duration<float>(0.5f);
    std::chrono::time_point<__typeof__ main_clock> last_log_time;
    bool force_log = false;

    std::vector<float> current;
    std::vector<float> best_arg;
    R best_result;
    bool any_valid = false;

    float bounds_scale;
    size_t sample_rounds;
    float stepping_scale;

    float step_scale = 1.f;

    optimiser(std::function<R(const std::vector<float>&, T)> func,
              const std::vector<float>& lowerb,
              const std::vector<float>& upperb,
              const std::vector<float>& i_lin_step,
              const std::vector<float>& i_exp_step,
              bool maxm,
              T i_args,
              std::chrono::duration<float> i_max_duration,
              size_t rounds,
              float bounds_scale = 0.75f,
              float stepping_scale = 0.5f,
              size_t log_level = LOG_NONE)
    :
              funct(func),
              args(current, i_args),
              lower_bounds(lowerb),
              upper_bounds(upperb),
              min_lin_step(i_lin_step),
              min_exp_step(i_exp_step),
              maximise(maxm),
              max_duration(i_max_duration),
              log_level(log_level),
              bounds_scale(bounds_scale),
              sample_rounds(rounds),
              stepping_scale(stepping_scale) {

        size_t misize = std::min({lowerb.size(), upperb.size(), i_lin_step.size(), i_exp_step.size()});
        size_t masize = std::max({lowerb.size(), upperb.size(), i_lin_step.size(), i_exp_step.size()});
        if (misize != masize) {
            throw std::runtime_error("optimiser parameters have mismatched dimensions");
        }

        for (size_t i = 0; i < masize; ++i) {
            if (lowerb[i] > upperb[i]) {
                throw std::runtime_error("optimiser upper bound " + std::to_string(i) + " was smaller than lower bound");
            }
        }

        reset();
    }

    void reset() {
        current = lower_bounds;
        any_valid = false;
        step_scale = 1.f;

        sample_count = last_sample_count = 0;
        last_log_time = main_clock.now();
    }

    void find_best() {
        size_t paramc = current.size();
        std::vector<float> cur_lower_bounds = lower_bounds;
        std::vector<float> cur_upper_bounds = upper_bounds;
        step_scale = 1.f;
        for (size_t samp_idx = 0; samp_idx < sample_rounds; ++samp_idx) {
            std::chrono::time_point s_time = main_clock.now();
            while (main_clock.now() - s_time < max_duration) {
                // start off a random point in the dimension space
                for (size_t i = 0; i < paramc; ++i) {
                    current[i] = cur_lower_bounds[i] + (cur_upper_bounds[i] - cur_lower_bounds[i]) * frand();
                }
                // do gradient descent until we find a local minimum
                R c_result = sample();
                ++init_sample_count;
                log([&](){ return std::format("Doing starting sample on [{}], result {}", vec_to_str(current), c_result.rating()); }, log_level, LOG_TRACE);
                while (true) {
                    // movement directions yielding best result
                    std::vector<std::pair<size_t, bool>> best_movedirs = {};
                    R best_move_res = c_result;
                    bool is_initial = true;
                    std::vector<float> old_current = current;
                    // sample each possible movement direction in the parameter space
                    for (size_t i = 0; i < paramc; ++i) {
                        log([&](){ return std::format("Trying to move in direction {}", i); }, log_level, LOG_TRACE);
                        auto do_update = [&](const R& with, bool sign) {
                            if (!is_initial && eq_to(with, best_move_res)) {
                                best_movedirs.push_back({i, sign});
                            } else if (better_than(with, best_move_res)) {
                                best_movedirs = {{i, sign}};
                                best_move_res = with;
                                is_initial = false;
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
                        log([&]() { return std::format("Local minimum found with rating {}", c_result.rating()); }, log_level, LOG_DEBUG);
                        break;
                    }

                    std::pair<size_t, float> chosen;
                    size_t chosen_scl = 0;
                    // try skip-move in each prospective movement direction
                    for (const std::pair<size_t, bool>& p : best_movedirs) {
                        float sign = p.second ? +1.f : -1.f;
                        // check if we can skip-move in this direction
                        // the function handles checking whether that'd actually be profitable
                        size_t dir = p.first;
                        size_t scl = 0;
                        for (size_t move_scl = 2; true; move_scl *= 2) {
                            log([&](){ return std::format("Trying to move in direction {} with scale {}", dir, sign * move_scl); }, log_level, LOG_TRACE);
                            // step forward in chosen direction with scaling
                            current[dir] += sign * get_step(dir, move_scl);
                            // don't try to sample beyond bounds
                            if (current[dir] < cur_lower_bounds[dir] || current[dir] > cur_upper_bounds[dir]) {
                                // reset
                                current[dir] = old_current[dir];
                                break;
                            }
                            // sample and check if scaling the movement produced better results
                            R res = sample();
                            if (better_than(res, best_move_res)) {
                                chosen_scl = move_scl;
                                best_move_res = res;
                            } else {
                                current[dir] = old_current[dir];
                                break;
                            }
                            current[dir] = old_current[dir];
                        }
                        if (scl != 0) {
                            chosen = {p.first, sign};
                            chosen_scl = scl;
                        }
                    }
                    // we failed to find any non-zero movement, break to avoid random walk
                    if (chosen_scl == 0 || eq_to(best_move_res, c_result)) {
                        log([&](){ return std::format("Local minimum found with rating {}", c_result.rating()); }, log_level, LOG_DEBUG);
                        break;
                    }
                    // perform the movement
                    current[chosen.first] += chosen.second * get_step(chosen.first, chosen_scl);
                    log([&](){ return std::format("Moving from {} -> {}", c_result.rating(), best_move_res.rating()); }, log_level, LOG_TRACE);
                    c_result = best_move_res;
                }
            }
            if (!any_valid) {
                log([&](){ return "Failed to find any viable result, retrying sample 1..."; }, log_level, LOG_BASIC);
                --samp_idx;
                s_time = main_clock.now();
                continue;
            }
            if (samp_idx + 1 != sample_rounds) {
                log([&]() { return std::format("Sampling round {} complete, best: {}", samp_idx + 1, best_result.rating()); }, log_level, LOG_BASIC);
                force_log = true;

                // sampling round done, halve sampling area and go again
                for (size_t i = 0; i < current.size(); ++i) {
                    float& lowerb = cur_lower_bounds[i];
                    float& upperb = cur_upper_bounds[i];
                    const float& best_at = best_arg[i];
                    float c_scale = std::pow(bounds_scale, samp_idx + 1);
                    lowerb = lower_bounds[i] + (best_at - lower_bounds[i]) * (1.f - c_scale);
                    upperb = upper_bounds[i] + (best_at - upper_bounds[i]) * (1.f - c_scale);
                    // scale stepping less
                    step_scale *= stepping_scale;
                }
                log([&](){ return std::format("New bounds: [{}] to [{}]", vec_to_str(cur_lower_bounds), vec_to_str(cur_upper_bounds)); }, log_level, LOG_INFO);
            }
        }

        log([&]() { return std::format("Finished with {} ({}) samples", sample_count, init_sample_count); }, log_level, LOG_BASIC);
    }

    // returns pair of sign-adjusted result and whether this updated our maximum
    R sample() {
        ++sample_count;
        if (log_level >= LOG_INFO) {
            auto now = main_clock.now();
            std::chrono::duration<float> tdiff = now - last_log_time;
            if (tdiff > log_spacing || force_log) {
                float sec = tdiff.count();
                log([&](){ return std::format("{} ({} init) Samples ({:.0f} ({:.0f}) samples/s)",
                                              sample_count, init_sample_count,
                                              (sample_count - last_sample_count) / sec, (init_sample_count - last_init_sample_count) / sec);
                }, log_level, LOG_INFO, false);
                std::flush(std::cout);
                last_sample_count = sample_count;
                last_init_sample_count = init_sample_count;
                last_log_time = now;
                force_log = false;
            }
        }
        R res = apply(funct, args);
        bool valid = res.valid();
        any_valid |= valid;

        if (better_than(res, best_result)) {
            best_result = res;
            best_arg = current;
        }

        log([&](){ return std::format("Sampled {}, result {}", vec_to_str(current), res.rating()); }, log_level, LOG_TRACE);

        return res;
    }

    bool better_than(const R& what, const R& than) {
        if (!than.valid()) return what.valid();
        if (!what.valid()) return false;
        return maximise ? what > than : than > what;
    }

    bool better_eq_than(const R& what, const R& than) {
        if (!than.valid()) return true;
        if (!what.valid()) return !than.valid();
        return maximise ? what >= than : than >= what;
    }

    bool eq_to(const R& what, const R& than) {
        if (!than.valid()) return !what.valid();
        if (!what.valid()) return !than.valid();
        return what == than;
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

}

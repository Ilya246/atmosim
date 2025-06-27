#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <vector>

#include "utility.hpp"
#include "constants.hpp"

namespace asim {

template<typename T, typename R>
struct optimiser {
    // generic optimiser configuration
    std::function<R(const std::vector<float>&, const T&)> funct;
    T args;
    std::vector<float> lower_bounds;
    std::vector<float> upper_bounds;
    bool maximise;
    std::chrono::duration<float> max_duration;
    size_t log_level;

    // specific optimiser configuration
    float bounds_scale;
    size_t sample_rounds;
    float base_step = 0.001f;
    float adapt_noise = 0.5f;
    float orth_strength = 0.2f;
    float move_scaling = 1.5f;
    // each how many vector adjustments should we try to space them apart
    size_t orth_interval = 100;
    size_t fuzzn = 10000;

    // state
    bool do_adapt;
    size_t sample_count = 0, last_sample_count = 0;
    size_t valid_sample_count = 0, last_valid_sample_count = 0;
    std::chrono::duration<float> log_spacing = std::chrono::duration<float>(0.05f);
    std::chrono::time_point<__typeof__ main_clock> last_log_time;
    float speed_iters, speed_valid_iters;
    std::chrono::duration<float> speed_log_spacing = std::chrono::duration<float>(0.5f);
    std::chrono::time_point<__typeof__ main_clock> last_speed_update_time;
    bool force_log = false;
    std::vector<float> cur_lower_bounds;
    std::vector<float> cur_upper_bounds;
    size_t adapt_counter = 0;

    std::vector<float> best_arg;
    R best_result;
    bool any_valid = false;

    // inter-round state
    float step_scale = 1.f;

    std::vector<std::vector<float>> search_directions;
    // dimensions we don't want to be stepping in
    std::vector<bool> fixed_dims;

    optimiser(std::function<R(const std::vector<float>&, T)> func,
              const std::vector<float>& lowerb,
              const std::vector<float>& upperb,
              bool maxm,
              T i_args,
              std::chrono::duration<float> i_max_duration,
              size_t rounds,
              float bounds_scale = 0.75f,
              size_t log_level = LOG_NONE)
    :
              funct(func),
              args(i_args),
              lower_bounds(lowerb),
              upper_bounds(upperb),
              maximise(maxm),
              max_duration(i_max_duration),
              log_level(log_level),
              bounds_scale(bounds_scale),
              sample_rounds(rounds) {

        size_t misize = std::min({lowerb.size(), upperb.size()});
        size_t masize = std::max({lowerb.size(), upperb.size()});
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

    void initialize_directions() {
        search_directions.clear();
        // Create initial directions only for free dimensions
        size_t dims = fixed_dims.size();
        for(size_t i = 0; i < dims; ++i) {
            if(!fixed_dims[i]) {
                float v_size = (upper_bounds[i] - lower_bounds[i]) * base_step;
                search_directions.emplace_back(dims, 0.f);
                search_directions.back()[i] = v_size;
                log([&](){ return std::format("Initialised step vector [{}]", vec_to_str(search_directions.back())); }, log_level, LOG_DEBUG);
                search_directions.emplace_back(dims, 0.f);
                search_directions.back()[i] = -v_size;
                log([&](){ return std::format("Initialised step vector [{}]", vec_to_str(search_directions.back())); }, log_level, LOG_DEBUG);
            }
        }
    }

    void reset() {
        size_t dims = lower_bounds.size();
        // Identify fixed dimensions
        fixed_dims.resize(dims);
        for(size_t i = 0; i < dims; ++i) {
            fixed_dims[i] = (lower_bounds[i] == upper_bounds[i]);
        }

        do_adapt = dims > 1;
        any_valid = false;
        step_scale = 1.f;

        sample_count = last_sample_count = 0;
        last_log_time = main_clock.now();
        initialize_directions();
    }

    void find_best() {
        cur_lower_bounds = lower_bounds;
        cur_upper_bounds = upper_bounds;

        for (size_t samp_idx = 0; samp_idx < sample_rounds; ++samp_idx) {
            if (status_SIGINT) break;

            std::chrono::time_point s_time = main_clock.now();
            while (main_clock.now() - s_time < max_duration) {
                if (status_SIGINT) break;

                std::vector<float> current = random_vec(cur_lower_bounds, cur_upper_bounds);
                log([&](){ return std::format("Doing initial sample at {}", vec_to_str(current)); }, log_level, LOG_TRACE);
                R c_result = sample(current);
                if (!c_result.valid()) {
                    log([&](){ return "Initial sample invalid, aborting"; }, log_level, LOG_TRACE);
                    continue;
                }

                float move_scl = 1.f;
                bool is_scaled = false;
                while (true) {
                    struct step_candidate {
                        size_t dir_index;
                        R result;
                    };
                    bool do_end = false, recheck = false;
                    do {
                        recheck = false;
                        std::vector<step_candidate> candidates;

                        log([&](){ return "Checking search directions"; }, log_level, LOG_TRACE);
                        // Generate candidate steps in all search directions
                        size_t dirs = search_directions.size();
                        for(size_t dir_idx = 0; dir_idx < dirs; ++dir_idx) {
                            const std::vector<float>& dir = search_directions[dir_idx];
                            std::vector<float> move_dir = dir * move_scl;
                            std::vector<float> candidate = current + move_dir;
                            // check bounds
                            if (!vec_in_bounds(candidate, cur_lower_bounds, cur_upper_bounds)) continue;

                            log([&](){ return std::format("Sampling candidate offset by {}", vec_to_str(move_dir)); }, log_level, LOG_TRACE);
                            candidates.emplace_back(dir_idx, sample(candidate));
                        }

                        // Find best candidate
                        auto best_it = std::max_element(candidates.begin(), candidates.end(),
                            [this](const auto& a, const auto& b) { return better_than(b.result, a.result); });
                        if (best_it == candidates.end() || !better_than(best_it->result, c_result)) {
                            do_end = !is_scaled;
                            recheck = is_scaled;
                            is_scaled = false;
                            move_scl = 1.f;
                            continue; // Local minimum found
                        }
                        move_scl *= move_scaling;

                        std::vector<float>& best_dir = search_directions[best_it->dir_index];
                        log([&](){ return std::format("Best direction {} found, result {} vs {}", vec_to_str(best_dir), best_it->result.rating(), c_result.rating()); }, log_level, LOG_TRACE);
                        if (!do_adapt || is_scaled) {
                            current += best_dir;
                            c_result = best_it->result;
                            is_scaled = true;
                            continue;
                        }
                        is_scaled = true;

                        // attempt to randomly improve current movement vector
                        std::vector<float> dir_improv_candidate = (random_vec(cur_lower_bounds, cur_upper_bounds) - cur_lower_bounds) * (base_step * adapt_noise);
                        dir_improv_candidate += best_dir;
                        dir_improv_candidate *= length(best_dir) / length(dir_improv_candidate);

                        std::vector<float> improv_candidate = current;
                        improv_candidate = current + dir_improv_candidate;
                        if (!vec_in_bounds(improv_candidate, cur_lower_bounds, cur_upper_bounds)) {
                            current += best_dir;
                            c_result = best_it->result;
                            continue;
                        }

                        R rotated_result = sample(improv_candidate);
                        // also update the search direction if we found a better one
                        if(better_than(rotated_result, best_it->result)) {
                            current = improv_candidate;
                            c_result = rotated_result;
                            log([&](){ return std::format("Improved direction {} [{}] -> [{}]",
                                best_it->dir_index, vec_to_str(best_dir), vec_to_str(dir_improv_candidate)); }, log_level, LOG_DEBUG);
                            best_dir = dir_improv_candidate;
                            ++adapt_counter;
                            if (adapt_counter > orth_interval) {
                                log([&](){ return "Orthogonalising search vectors, current:"; }, log_level, LOG_DEBUG);
                                if (log_level >= LOG_DEBUG) {
                                    for (const std::vector<float>& dir : search_directions) log([&](){ return std::format("[{}]", vec_to_str(dir)); }, log_level, LOG_DEBUG);
                                }
                                space_vectors(search_directions, orth_strength);
                                log([&](){ return "New:"; }, log_level, LOG_DEBUG);
                                if (log_level >= LOG_DEBUG) {
                                    for (const std::vector<float>& dir : search_directions) log([&](){ return std::format("[{}]", vec_to_str(dir)); }, log_level, LOG_DEBUG);
                                }
                                adapt_counter = 0;
                            }
                        } else {
                            current += best_dir;
                            c_result = best_it->result;
                        }
                    } while (recheck);
                    if (do_end) break;
                }
            }

            if (!any_valid) {
                log([&](){ return "Failed to find any viable result, retrying sample 1..."; }, log_level, LOG_BASIC);
                --samp_idx;
                continue;
            }

            // try enhancing our best result
            for (size_t f_i = 0; f_i < fuzzn; ++f_i) {
                std::vector<float> rv = (random_vec(cur_lower_bounds, cur_upper_bounds) - cur_lower_bounds);
                std::vector<float> fuzz_coord = best_arg + rv * (base_step * frand());
                if (!vec_in_bounds(fuzz_coord, cur_lower_bounds, cur_upper_bounds)) continue;
                sample(fuzz_coord);
            }

            if (samp_idx + 1 != sample_rounds) {
                log([&]() { return std::format("Sampling round {} complete, best: {}", samp_idx + 1, best_result.rating_str()); }, log_level, LOG_BASIC);
                force_log = true;

                // update bounds and directions
                float c_scale = std::pow(bounds_scale, samp_idx + 1);

                cur_lower_bounds = lerp(lower_bounds, best_arg, 1.f - c_scale);
                cur_upper_bounds = lerp(upper_bounds, best_arg, 1.f - c_scale);
                // also downscale search directions
                for (std::vector<float>& v : search_directions) v *= bounds_scale;

                log([&](){ return std::format("New bounds: [{}] to [{}]", vec_to_str(cur_lower_bounds), vec_to_str(cur_upper_bounds)); }, log_level, LOG_INFO);
            }
        }

        log([&]() { return std::format("Finished with {} ({}) samples", sample_count, valid_sample_count); }, log_level, LOG_BASIC);
    }

    R sample(const std::vector<float>& at) {
        if (log_level >= LOG_INFO) {
            auto now = main_clock.now();
            std::chrono::duration<float> tdiff = now - last_log_time;
            if ((tdiff > log_spacing || force_log)) {
                std::chrono::duration<float> speed_tdiff = now - last_speed_update_time;
                if (speed_tdiff > speed_log_spacing) {
                    float sec = speed_tdiff.count();
                    speed_iters = (sample_count - last_sample_count) / sec;
                    speed_valid_iters = (valid_sample_count - last_valid_sample_count) / sec,
                    last_sample_count = sample_count;
                    last_valid_sample_count = valid_sample_count;
                    last_speed_update_time = now;
                }
                last_log_time = now;
                log([&](){ return std::format("{} ({} valid) Samples ({:.0f} ({:.0f}) samples/s), best: {}",
                                              sample_count, valid_sample_count,
                                              speed_iters, speed_valid_iters,
                                              best_result.rating());
                }, log_level, LOG_INFO, false);
                std::flush(std::cout);
                force_log = false;
            }
        }
        R res = apply(funct, std::tie(at, args));

        any_valid |= res.valid();
        ++sample_count;
        valid_sample_count += res.valid();
        log([&](){ return std::format("Sampled {}, result {}", vec_to_str(at), res.rating_str()); }, log_level, LOG_TRACE);
        if (better_than(res, best_result)) {
            log([&](){ return std::format("Updating best from {}", best_result.rating_str()); }, log_level, LOG_DEBUG);
            best_result = res;
            best_arg = at;
        }

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
};

}

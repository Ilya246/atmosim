#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <thread>
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
    duration_t max_duration;
    size_t log_level;
    size_t n_threads = 1;

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
    duration_t poll_spacing = std::chrono::duration_cast<duration_t>(std::chrono::duration<float>(0.1f));
    duration_t speed_log_spacing = std::chrono::duration_cast<duration_t>(std::chrono::duration<float>(0.5f));

    // state
    time_point_t last_poll_time;
    time_point_t last_speed_update_time;
    bool do_adapt;
    // inter-round state
    float step_scale = 1.f;
    std::vector<float> best_arg;
    R best_result;
    // dimensions we don't want to be stepping in
    std::vector<bool> fixed_dims;

    optimiser(std::function<R(const std::vector<float>&, T)> func,
              const std::vector<float>& lowerb,
              const std::vector<float>& upperb,
              bool maxm,
              T i_args,
              duration_t i_max_duration,
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

    void reset() {
        size_t dims = lower_bounds.size();
        // Identify fixed dimensions
        fixed_dims.resize(dims);
        size_t non_fixed_dims = 0;
        for(size_t i = 0; i < dims; ++i) {
            fixed_dims[i] = (lower_bounds[i] == upper_bounds[i]);
            non_fixed_dims += !fixed_dims[i];
        }
        // don't adapt step vectors if we only have one direction we can actually step in
        do_adapt = non_fixed_dims > 1;

        step_scale = 1.f;

        last_poll_time = main_clock.now();
        last_speed_update_time = main_clock.now();
    }

    struct sampler {
        const optimiser<T, R>& parent;

        // copies of parent state
        size_t log_level;
        bool maximise, do_adapt;
        float move_scaling, base_step, adapt_noise;

        // state
        std::vector<float> best_arg;
        R best_result;

        std::thread worker;

        std::vector<std::vector<float>> search_directions;
        std::vector<float> cur_lower_bounds;
        std::vector<float> cur_upper_bounds;
        size_t adapt_counter = 0;

        // state for logging
        size_t sample_count = 0;
        size_t valid_sample_count = 0;

        sampler(const optimiser<T, R>& parent): parent(parent) {}

        void reset(const std::vector<float>& lower_bounds, const std::vector<float>& upper_bounds) {
            log_level = parent.log_level;
            maximise = parent.maximise;
            do_adapt = parent.do_adapt;
            move_scaling = parent.move_scaling;
            base_step = parent.base_step;
            adapt_noise = parent.adapt_noise;
            cur_lower_bounds = lower_bounds;
            cur_upper_bounds = upper_bounds;
            adapt_counter = 0;
            sample_count = 0;
            valid_sample_count = 0;
        }

        void sample_until(time_point_t until) {
            worker = std::thread(__sample_until, this, until);
        }

        void join() {
            worker.join();
        }

        static void __sample_until(sampler* samp, time_point_t until) {
            sampler& self = *samp;
            while (main_clock.now() < until && !status_SIGINT) {
                self.do_sampling();
            }
        }

        void do_sampling() {
            std::vector<float> current = random_vec(cur_lower_bounds, cur_upper_bounds);
            log([&](){ return std::format("Doing initial sample at {}", vec_to_str(current)); }, log_level, LOG_TRACE);
            R c_result = sample(current);
            if (!c_result.valid()) {
                log([&](){ return "Initial sample invalid, aborting"; }, log_level, LOG_TRACE);
                return;
            }

            float move_scl = 1.f;
            bool is_scaled = false;
            while (!status_SIGINT) {
                struct step_candidate {
                    size_t dir_index;
                    R result;
                };
                bool recheck = false;
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
                        [this](const auto& a, const auto& b) { return parent.better_than(b.result, a.result, maximise); });
                    if (best_it == candidates.end() || !parent.better_than(best_it->result, c_result, maximise)) {
                        if (!is_scaled) return;
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
                    if(parent.better_than(rotated_result, best_it->result, maximise)) {
                        current = improv_candidate;
                        c_result = rotated_result;
                        log([&](){ return std::format("Improved direction {} [{}] -> [{}]",
                            best_it->dir_index, vec_to_str(best_dir), vec_to_str(dir_improv_candidate)); }, log_level, LOG_DEBUG);
                        best_dir = dir_improv_candidate;
                        ++adapt_counter;
                        if (adapt_counter > parent.orth_interval) {
                            log([&](){ return "Orthogonalising search vectors, current:"; }, log_level, LOG_DEBUG);
                            if (log_level >= LOG_DEBUG) {
                                for (const std::vector<float>& dir : search_directions) log([&](){ return std::format("[{}]", vec_to_str(dir)); }, log_level, LOG_DEBUG);
                            }
                            space_vectors(search_directions, parent.orth_strength);
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
            }
        }

        R sample(const std::vector<float>& at) {
            R res = apply(parent.funct, std::tie(at, parent.args));

            ++sample_count;
            valid_sample_count += res.valid();
            log([&](){ return std::format("Sampled {}, result {}", vec_to_str(at), res.rating_str()); }, log_level, LOG_TRACE);
            if (parent.better_than(res, best_result, maximise)) {
                log([&](){ return std::format("Updating best from {}", best_result.rating_str()); }, log_level, LOG_DEBUG);
                best_result = res;
                best_arg = at;
            }

            return res;
        }
    };

    std::vector<std::vector<float>> base_search_directions() {
        std::vector<std::vector<float>> out_vec;
        // Create initial directions only for free dimensions
        size_t dims = fixed_dims.size();
        for(size_t i = 0; i < dims; ++i) {
            if(!fixed_dims[i]) {
                float v_size = (upper_bounds[i] - lower_bounds[i]) * base_step;
                out_vec.emplace_back(dims, 0.f);
                out_vec.back()[i] = v_size;
                log([&](){ return std::format("Initialised step vector [{}]", vec_to_str(out_vec.back())); }, log_level, LOG_DEBUG);
                out_vec.emplace_back(dims, 0.f);
                out_vec.back()[i] = -v_size;
                log([&](){ return std::format("Initialised step vector [{}]", vec_to_str(out_vec.back())); }, log_level, LOG_DEBUG);
            }
        }

        return out_vec;
    }

    void find_best() {
        std::vector<sampler> samplers;
        std::vector<std::vector<float>> search_directions = base_search_directions();
        for (size_t i = 0; i < n_threads; ++i) {
            samplers.emplace_back(*this);
        }

        bool any_valid = false;
        size_t sample_count = 0, valid_sample_count = 0;
        size_t last_sample_count = 0, last_valid_sample_count = 0;
        float speed_iters, speed_valid_iters;

        std::vector<float> cur_lower_bounds(lower_bounds);
        std::vector<float> cur_upper_bounds(upper_bounds);

        for (size_t samp_idx = 0; samp_idx < sample_rounds; ++samp_idx) {
            if (status_SIGINT) break;

            time_point_t s_time = main_clock.now();
            while (main_clock.now() - s_time < max_duration) {
                if (status_SIGINT) break;

                // run samplers
                auto from = main_clock.now();
                for (sampler& samp : samplers) {
                    samp.reset(cur_lower_bounds, cur_upper_bounds);
                    samp.best_arg = best_arg;
                    samp.best_result = best_result;
                    samp.search_directions = search_directions;
                    samp.sample_until(from + poll_spacing);
                }
                for (sampler& t : samplers) {
                    t.join();
                }

                // aggregate sampler data
                for (const sampler& samp : samplers) {
                    sample_count += samp.sample_count;
                    valid_sample_count += samp.valid_sample_count;
                    any_valid |= samp.best_result.valid();
                    if (better_than(samp.best_result, best_result, maximise)) {
                        best_result = samp.best_result;
                        best_arg = samp.best_arg;
                    }
                }

                // also aggregate search directions
                size_t dirs = search_directions.size();
                for (size_t d = 0; d < dirs; ++d) {
                    search_directions[d] = samplers[(size_t)frand(n_threads)].search_directions[d];
                }

                if (log_level >= LOG_INFO) {
                    auto now = main_clock.now();
                    duration_t speed_tdiff = now - last_speed_update_time;
                    if (speed_tdiff > speed_log_spacing) {
                        float sec = std::chrono::duration_cast<std::chrono::duration<float>>(speed_tdiff).count();
                        speed_iters = (sample_count - last_sample_count) / sec;
                        speed_valid_iters = (valid_sample_count - last_valid_sample_count) / sec,
                        last_sample_count = sample_count;
                        last_valid_sample_count = valid_sample_count;
                        last_speed_update_time = now;
                    }
                    last_poll_time = now;
                    log([&](){ return std::format("{} ({} valid) Samples ({:.0f} ({:.0f}) samples/s), best: {}",
                                                sample_count, valid_sample_count,
                                                speed_iters, speed_valid_iters,
                                                best_result.rating());
                    }, log_level, LOG_INFO, false);
                    std::flush(std::cout);
                }
            }

            if (!any_valid) {
                log([&](){ return "Failed to find any viable result, retrying sample 1..."; }, log_level, LOG_BASIC);
                --samp_idx;
                continue;
            }

            // try enhancing our best result
            for (size_t f_i = 0; f_i < fuzzn; ++f_i) {
                sampler t_samp(*this);
                std::vector<float> rv = (random_vec(cur_lower_bounds, cur_upper_bounds) - cur_lower_bounds);
                std::vector<float> fuzz_coord = best_arg + rv * (base_step * frand());
                if (!vec_in_bounds(fuzz_coord, cur_lower_bounds, cur_upper_bounds)) continue;
                t_samp.sample(fuzz_coord);
            }

            if (samp_idx + 1 != sample_rounds) {
                log([&]() { return std::format("Sampling round {} complete, best: {}", samp_idx + 1, best_result.rating_str()); }, log_level, LOG_BASIC);

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

    static bool better_than(const R& what, const R& than, bool maximise) {
        if (!than.valid()) return what.valid();
        if (!what.valid()) return false;
        return maximise ? what > than : than > what;
    }

    static bool better_eq_than(const R& what, const R& than, bool maximise) {
        if (!than.valid()) return true;
        if (!what.valid()) return !than.valid();
        return maximise ? what >= than : than >= what;
    }

    static bool eq_to(const R& what, const R& than) {
        if (!than.valid()) return !what.valid();
        if (!what.valid()) return !than.valid();
        return what == than;
    }
};

}

#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <format>
#include <functional>
#include <iostream>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

#include "utility.hpp"

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

    // DE Parameters
    // Population size: usually 10x dimension, but we clamp for performance/time trade-off
    size_t pop_size = 50;
    // Differential weight (0.5 - 1.0)
    float mutation_factor = 0.6f;
    // Crossover probability (0.8 - 1.0)
    float crossover_prob = 0.9f;

    // Reporting
    duration_t poll_spacing = as_seconds(0.025f);
    duration_t speed_log_spacing = as_seconds(0.5f);

    // State
    time_point_t last_poll_time;
    time_point_t last_speed_update_time;

    // Inter-round state
    std::vector<float> best_arg;
    R best_result;

    // Dimensions we don't want to be stepping in
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

        // Auto-tune population size based on dimensions, clamped
        size_t dims = lowerb.size();
        pop_size = std::max((size_t)20, std::min((size_t)100, dims * 15));

        reset();
    }

    void reset() {
        size_t dims = lower_bounds.size();
        // Identify fixed dimensions
        fixed_dims.resize(dims);
        for(size_t i = 0; i < dims; ++i) {
            fixed_dims[i] = (lower_bounds[i] == upper_bounds[i]);
        }

        last_poll_time = main_clock.now();
        last_speed_update_time = main_clock.now();
    }

    struct sampler {
        const optimiser<T, R>& parent;

        // copies of parent state
        size_t log_level;
        bool maximise;

        // DE Parameters local copy
        size_t pop_size;
        float F, CR;

        // state
        std::vector<float> best_arg;
        R best_result;

        time_point_t until;

        std::atomic<bool> should_terminate{false};
        std::atomic<bool> running{false};
        std::unique_ptr<std::thread> worker = nullptr;
        std::mutex ready_mutex;
        std::condition_variable cv;

        // state fed to us
        std::string worker_prefix = "";
        std::vector<float> cur_lower_bounds;
        std::vector<float> cur_upper_bounds;

        // state for logging
        std::atomic<size_t> sample_count{0};
        std::atomic<size_t> valid_sample_count{0};

        // RNG
        std::mt19937 rng;

        sampler(const optimiser<T, R>& parent, int index = -1, bool do_threading = true)
            : parent(parent), rng(std::random_device{}()) {

            if (index >= 0) {
                worker_prefix = std::format("[{}]: ", index);
            }

            if (do_threading) {
                worker = std::make_unique<std::thread>([this] {
                    std::mutex mutex;
                    std::unique_lock lock(mutex);
                    while (true) {
                        cv.wait(lock, [this]{ return running.load() || should_terminate.load() || status_SIGINT; });
                        if (should_terminate.load() || status_SIGINT) break;

                        ready_mutex.lock();
                        while (main_clock.now() < until) {
                            if(status_SIGINT) break;
                            do_sampling();
                        }
                        ready_mutex.unlock();

                        running = false;
                    }
                });
            }
        }

        ~sampler() {
            if (worker) {
                running = false;
                should_terminate = true;
                cv.notify_all();
                worker->join();
            }
        }

        void reset(const std::vector<float>& lower_bounds, const std::vector<float>& upper_bounds) {
            log_level = parent.log_level;
            maximise = parent.maximise;

            pop_size = parent.pop_size;
            F = parent.mutation_factor;
            CR = parent.crossover_prob;

            best_arg = parent.best_arg;
            best_result = parent.best_result;
            cur_lower_bounds = lower_bounds;
            cur_upper_bounds = upper_bounds;
        }

        void start_sampling(time_point_t until) {
            this->until = until;
            running = true;
            if (worker) {
                cv.notify_one();
            } else {
                while (main_clock.now() < until) {
                    if(status_SIGINT) break;
                    do_sampling();
                }
                running = false;
            }
        }

        void wait_ready() {
            ready_mutex.lock();
            ready_mutex.unlock();
        }

        void do_sampling() {
            // Differential Evolution Implementation
            size_t dims = cur_lower_bounds.size();

            // Population initialization
            std::vector<std::vector<float>> population(pop_size);
            std::vector<R> fitness(pop_size);

            std::uniform_real_distribution<float> dist01(0.f, 1.f);

            // if we already have a best result, keep it as the first element of the population
            size_t start = 0;
            if (best_result.valid()) {
                population[0] = best_arg;
                fitness[0] = best_result;
                start = 1;
            }

            // 1. Initialize Population
            for (size_t i = start; i < pop_size; ++i) {
                population[i] = random_vec(cur_lower_bounds, cur_upper_bounds);
                fitness[i] = sample(population[i]);
            }

            std::vector<float> trial(dims);

            // 2. Evolution Loop
            // We run generation by generation until the 'until' time is hit
            // The outer loop in sampler handles the timing check

            while (main_clock.now() < until && !status_SIGINT) {
                for (size_t i = 0; i < pop_size; ++i) {
                    // Pick 3 distinct random indices (a, b, c) != i
                    size_t a, b, c;
                    do { a = std::uniform_int_distribution<size_t>(0, pop_size - 1)(rng); } while(a == i);
                    do { b = std::uniform_int_distribution<size_t>(0, pop_size - 1)(rng); } while(b == i || b == a);
                    do { c = std::uniform_int_distribution<size_t>(0, pop_size - 1)(rng); } while(c == i || c == a || c == b);

                    // Mutation & Crossover
                    // DE/rand/1/bin strategy
                    // Mutant = a + F * (b - c)
                    size_t R_idx = std::uniform_int_distribution<size_t>(0, dims - 1)(rng);

                    for (size_t j = 0; j < dims; ++j) {
                        if (parent.fixed_dims[j]) {
                            trial[j] = cur_lower_bounds[j];
                            continue;
                        }

                        if (frand() < CR || j == R_idx) {
                            float val = population[a][j] + F * (population[b][j] - population[c][j]);
                            // Bound handling: Clamp
                            val = std::max(cur_lower_bounds[j], std::min(cur_upper_bounds[j], val));
                            trial[j] = val;
                        } else {
                            trial[j] = population[i][j];
                        }
                    }

                    // Selection
                    R trial_res = sample(trial);

                    if (parent.better_eq_than(trial_res, fitness[i], maximise)) {
                        population[i] = trial;
                        fitness[i] = trial_res;
                    }
                }
            }
        }

        R sample(const std::vector<float>& at) {
            R res = parent.funct(at, parent.args);

            ++sample_count;
            valid_sample_count += res.valid();

            // Check against local best
            if (parent.better_than(res, best_result, maximise)) {
                // Log only occasionally or if significantly better to avoid spam
                log([&]{ return std::format("{}New local best: {}", worker_prefix, res.rating_str()); }, log_level, LOG_DEBUG);
                best_result = res;
                best_arg = at;
            }

            return res;
        }
    };

    void find_best() {
        std::vector<std::unique_ptr<sampler>> samplers;
        for (size_t i = 0; i < n_threads; ++i) {
            samplers.emplace_back(std::make_unique<sampler>(*this, i, n_threads != 1));
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
            // Divide total runtime by rounds
            duration_t round_duration = max_duration / sample_rounds;
            time_point_t end_time = s_time + round_duration;

            while (main_clock.now() < end_time) {
                if (status_SIGINT) break;

                time_point_t from = main_clock.now();
                time_point_t time_to = std::min(end_time, from + poll_spacing);

                for (std::unique_ptr<sampler>& samp : samplers) {
                    // Update bounds for the samplers (in case we tightened them previous round)
                    samp->reset(cur_lower_bounds, cur_upper_bounds);
                    samp->start_sampling(time_to);
                }

                // just sleep until the samplers are done
                std::this_thread::sleep_until(time_to);

                // aggregate sampler data
                for (const std::unique_ptr<sampler>& samp : samplers) {
                    samp->wait_ready();
                    sample_count += samp->sample_count;
                    valid_sample_count += samp->valid_sample_count;
                    samp->sample_count = 0;
                    samp->valid_sample_count = 0;
                    any_valid |= samp->best_result.valid();

                    if (better_than(samp->best_result, best_result, maximise)) {
                        best_result = samp->best_result;
                        best_arg = samp->best_arg;
                    }
                }

                if (log_level >= LOG_INFO) {
                    auto now = main_clock.now();
                    duration_t speed_tdiff = now - last_speed_update_time;
                    if (speed_tdiff > speed_log_spacing) {
                        float sec = to_seconds(speed_tdiff);
                        speed_iters = (sample_count - last_sample_count) / sec;
                        speed_valid_iters = (valid_sample_count - last_valid_sample_count) / sec,
                        last_sample_count = sample_count;
                        last_valid_sample_count = valid_sample_count;
                        last_speed_update_time = now;
                    }
                    last_poll_time = now;
                    log([&]{ return std::format("{} ({} valid) Samples ({:.0f} ({:.0f}) samples/s), best: {}",
                                                sample_count, valid_sample_count,
                                                speed_iters, speed_valid_iters,
                                                best_result.rating());
                    }, log_level, LOG_INFO, false);
                    std::flush(std::cout);
                }
            }

            if (!any_valid && samp_idx < sample_rounds - 1) {
                log([&]{ return "Failed to find any viable result this round, retrying..."; }, log_level, LOG_BASIC);
                continue;
            }

            // If we found something valid, update our best
            if (any_valid) {
                 log([&]() { return std::format("Sampling round {} complete, best: {}", samp_idx + 1, best_result.rating_str()); }, log_level, LOG_BASIC);

                if (samp_idx + 1 != sample_rounds) {
                    // Zooming Strategy:
                    // Contract bounds around the best known argument to refine precision
                    float c_scale = std::pow(bounds_scale, samp_idx + 1);

                    // Ensure we don't collapse to zero width on dimensions that need variation
                    for(size_t d=0; d<cur_lower_bounds.size(); ++d) {
                        if(fixed_dims[d]) continue;

                        float span = upper_bounds[d] - lower_bounds[d];
                        float current_span = span * c_scale;

                        // Center around best arg, but clamp to original hard bounds
                        cur_lower_bounds[d] = std::max(lower_bounds[d], best_arg[d] - current_span / 2.f);
                        cur_upper_bounds[d] = std::min(upper_bounds[d], best_arg[d] + current_span / 2.f);
                    }

                    log([&]{ return std::format("New bounds: [{}] to [{}]", vec_to_str(cur_lower_bounds), vec_to_str(cur_upper_bounds)); }, log_level, LOG_INFO);
                }
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
};

}

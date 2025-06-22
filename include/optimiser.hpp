#include <chrono>
#include <cmath>
#include <functional>
#include <tuple>
#include <vector>

#include "utility.hpp"
#include "constants.hpp"

namespace asim {

template<typename T, typename R>
struct optimizer {
    std::function<R(const std::vector<float>&, T)> funct;
    std::tuple<const std::vector<float>&, T> args;
    const std::vector<float>& lower_bounds;
    const std::vector<float>& upper_bounds;
    const std::vector<float>& min_lin_step;
    const std::vector<float>& min_exp_step;
    bool maximise;
    std::chrono::duration<float> max_duration;

    std::vector<float> current;
    std::vector<float> best_arg;
    R best_result;
    bool any_valid = false;


    float bounds_scale;
    size_t sample_rounds;
    float stepping_scale;

    float step_scale = 1.f;

    optimizer(std::function<R(const std::vector<float>&, T)> func,
              const std::vector<float>& lowerb,
              const std::vector<float>& upperb,
              const std::vector<float>& i_lin_step,
              const std::vector<float>& i_exp_step,
              bool maxm,
              T i_args,
              std::chrono::duration<float> i_max_duration,
              size_t rounds,
              float bounds_scale = 0.75f,
              float stepping_scale = 0.5f)
    :
              funct(func),
              args(current, i_args),
              lower_bounds(lowerb),
              upper_bounds(upperb),
              min_lin_step(i_lin_step),
              min_exp_step(i_exp_step),
              maximise(maxm),
              max_duration(i_max_duration),
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

        current = lower_bounds; // copy

        best_arg = std::vector<float>(current.size(), 0.f);

        best_result = worst_res();
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
                while (true) {
                    /* if (log_level >= 3) {
                        cout << "Sampling: ";
                        for (float f : current) {
                            cout << f << " ";
                        }
                        cout << endl;
                    } */
                    // movement directions yielding best result
                    std::vector<std::pair<size_t, bool>> best_movedirs = {};
                    R best_move_res = c_result;
                    std::vector<float> old_current = current;
                    // sample each possible movement direction in the parameter space
                    for (size_t i = 0; i < paramc; ++i) {
                        auto do_update = [&](const R& with, bool sign) {
                            if (with == best_move_res) {
                                best_movedirs.push_back({i, sign});
                            } else if (better_than(with, best_move_res)) {
                                best_movedirs = {{i, sign}};
                                best_move_res = with;
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
                        /* if (log_level >= 2) {
                            bomb_data data = get_data(get<0>(args), get<1>(args));
                            print_bomb(data, "Local minimum found: ");
                        } */
                        break;
                    }

                    auto do_skipmove = [&](size_t dir, float sign) {
                        size_t chosen_scl = 0;
                        for (size_t move_scl = 2; true; move_scl *= 2) {
                            // step forward in chosen direction with scaling
                            current[dir] += sign * get_step(dir, move_scl);
                            // don't try to sample beyond bounds
                            if (current[dir] < cur_lower_bounds[dir] || current[dir] > cur_upper_bounds[dir]) {
                                // reset
                                current[dir] = old_current[dir];
                                break;
                            }
                            // sample and check if scaling the movement produced better or same results
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
                    std::pair<size_t, float> chosen;
                    size_t chosen_scl = 0;
                    // try skip-move in each prospective movement direction
                    for (const std::pair<size_t, bool>& p : best_movedirs) {
                        float sign = p.second ? +1.f : -1.f;
                        // check if we can skip-move in this direction
                        // the function handles checking whether that'd actually be profitable
                        size_t scl = do_skipmove(p.first, sign);
                        if (scl != 0) {
                            chosen = {p.first, sign};
                            chosen_scl = scl;
                        }
                    }
                    // we failed to find any non-zero movement, break to avoid random walk
                    if (chosen_scl == 0 || best_move_res == c_result) {
                        /* if (log_level >= 2) {
                            bomb_data data = get_data(get<0>(args), get<1>(args));
                            print_bomb(data, "Local minimum found: ");
                        } */
                        break;
                    }
                    // perform the movement
                    current[chosen.first] += chosen.second * get_step(chosen.first, chosen_scl);
                    c_result = best_move_res;
                }
            }
            if (!any_valid) {
                /* if (log_level >= 1) {
                    cout << "Failed to find any viable result, retrying sample 1..." << endl;
                } */
                --samp_idx;
                s_time = main_clock.now();
                continue;
            }
            if (samp_idx + 1 != sample_rounds) {
                /* if (log_level >= 1) {
                    cout << "\nSampling round " << samp_idx + 1 << " complete" << endl;
                    bomb_data data = get_data(best_arg, get<1>(args));
                    print_bomb(data, "Best so far: ");
                } */
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
                /* if (log_level >= 1) {
                    cout << "New bounds: ";
                    for (size_t i = 0; i < current.size(); ++i) {
                        cout << "[" << cur_lower_bounds[i] << "," << cur_upper_bounds[i] << "] ";
                    }
                    cout << endl;
                } */
            }
        }
    }

    // returns pair of sign-adjusted result and whether this updated our maximum
    R sample() {
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

    bool better_than(const R& what, const R& than) {
        return maximise ? what > than : than > what;
    }

    bool better_eq_than(const R& what, const R& than) {
        return maximise ? what >= than : than >= what;
    }

    const R worst_res() {
        return R::worst(maximise);
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

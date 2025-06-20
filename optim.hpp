#pragma once
#include <vector>
#include <string>
#include <tuple>
#include <functional>
#include <chrono>
#include "gas.hpp"
#include "tank.hpp"
#include "core.hpp"

struct bomb_data {
    std::vector<float> mix_ratios, primer_ratios;
    float fuel_temp, fuel_pressure, thir_temp, mix_pressure, mix_temp;
    std::vector<gas_type> mix_gases, primer_gases;
    tank_state state = intact;
    float radius = 0.0, fin_temp = -1.0, fin_pressure = -1.0, fin_heat_leak = -1.0, optstat = -1.0;
    int ticks = -1;
    bomb_data(std::vector<float> mix_ratios, std::vector<float> primer_ratios, float fuel_temp, float fuel_pressure, float thir_temp, float mix_pressure, float mix_temp, const std::vector<gas_type>& mix_gases, const std::vector<gas_type>& primer_gases);
    void results(float n_radius, float n_fin_temp, float n_fin_pressure, float n_optstat, int n_ticks, tank_state n_state);
    std::string print_very_simple() const;
    std::string print_simple() const;
    std::string print_extensive() const;
};

void print_bomb(const bomb_data& bomb, const std::string& what, bool extensive = false);

struct opt_val_wrap {
    float stat;
    float pressure;
    bool valid_v;
    static opt_val_wrap worst(bool maximise);
    bool valid() const;
    bool operator>(const opt_val_wrap& rhs) const;
    bool operator>=(const opt_val_wrap& rhs) const;
    bool operator==(const opt_val_wrap& rhs) const;
};

opt_val_wrap do_sim(const std::vector<float>& in_args, std::tuple<const std::vector<gas_type>&, const std::vector<gas_type>&, bool> args);
bomb_data get_data(const std::vector<float>& in_args, std::tuple<const std::vector<gas_type>&, const std::vector<gas_type>&, bool> args);
bomb_data test_mix(const std::vector<gas_type>& mix_gases, const std::vector<gas_type>& primer_gases, float mixt1, float mixt2, float thirt1, float thirt2, bool maximise, bool measure_before);

// Optimizer template
template<typename T, typename R>
struct optimizer {
    std::function<R(const std::vector<float>&, T)> funct;
    std::tuple<const std::vector<float>&, T> args;
    const std::vector<float>& lower_bounds;
    const std::vector<float>& upper_bounds;
    const std::vector<float>& min_lin_step;
    const std::vector<float>& min_exp_step;
    bool maximise;
    float max_duration; // seconds
    std::vector<float> current;
    std::vector<float> best_arg;
    R best_result;
    bool any_valid = false;
    float step_scale = 1.f;
    optimizer(std::function<R(const std::vector<float>&, T)> func,
              const std::vector<float>& lowerb,
              const std::vector<float>& upperb,
              const std::vector<float>& i_lin_step,
              const std::vector<float>& i_exp_step,
              bool maxm,
              T i_args,
              float i_max_duration = 3.0f);
    void find_best();
    R sample();
    bool better_than(R what, R than);
    bool better_eq_than(R what, R than);
    R worst_res();
    float get_step(int i, float scale = 1.f);
    void step(int i);
}; 
#include "sim.hpp"
#include <tuple>
#include <memory>
#include <stdexcept>

namespace asim {

std::string params_supported_str = "radius, ticks, temperature";

std::istream& operator>>(std::istream& stream, field_ref<bomb_data>& re) {
    std::string val;
    stream >> val;
    if (val == "radius") re = field_ref<bomb_data>(offsetof(bomb_data, fin_radius), field_ref<bomb_data>::float_f);
    else if (val == "ticks") re = field_ref<bomb_data>(offsetof(bomb_data, ticks), field_ref<bomb_data>::int_f);
    else if (val == "temperature") re = field_ref<bomb_data>(offsetof(bomb_data, tank.mix.temperature), field_ref<bomb_data>::float_f);
    else stream.setstate(std::ios_base::failbit);
    return stream;
}

opt_val_wrap do_sim(const std::vector<float>& in_args, std::tuple<const std::vector<gas_ref>&, const std::vector<gas_ref>&, bool, size_t, field_ref<bomb_data>> args) {
    // read input parameters
    float target_temp = in_args[0];
    float fuel_temp = in_args[1];
    float thir_temp = in_args[2];
    // invalid mix, abort early
    if ((target_temp > fuel_temp) == (target_temp > thir_temp)) {
        return {};
    }
    const std::vector<gas_ref>& mix_gases = std::get<0>(args);
    const std::vector<gas_ref>& primer_gases = std::get<1>(args);
    bool measure_before = std::get<2>(args);
    size_t tick_cap = std::get<3>(args);
    field_ref<bomb_data> optstat_ref = std::get<4>(args);

    // read gas ratios
    std::vector<float> mix_ratios(mix_gases.size(), 1.f);
    std::vector<float> primer_ratios(primer_gases.size(), 1.f);
    size_t mg_s = mix_gases.size() - 1;
    size_t pg_s = primer_gases.size() - 1;
    for (size_t i = 0; i < mg_s; ++i) {
        mix_ratios[i + 1] = in_args[3 + i];
    }
    for (size_t i = 0; i < pg_s; ++i) {
        primer_ratios[i + 1] = in_args[3 + mg_s + i];
    }

    std::vector<float> mix_fractions = get_fractions(mix_ratios);
    std::vector<float> primer_fractions = get_fractions(primer_ratios);

    // set up the tank
    gas_tank tank;

    // specific heat is heat capacity of 1mol and fractions sum up to 1mol
    float fuel_specheat = get_mix_heat_capacity(mix_gases, mix_fractions);
    float primer_specheat = get_mix_heat_capacity(primer_gases, primer_fractions);
    // to how much we want to fill the tank
    float fuel_pressure = (target_temp / thir_temp - 1.f) * pressure_cap / (fuel_specheat / primer_specheat - 1.f + target_temp * (1.f / thir_temp - fuel_specheat / primer_specheat / fuel_temp));
    tank.mix.canister_fill_to(mix_gases, mix_fractions, fuel_temp, fuel_pressure);
    tank.mix.canister_fill_to(primer_gases, primer_fractions, thir_temp, pressure_cap);
    float mix_pressure = tank.mix.pressure();

    // invalid mix, abort
    if (fuel_pressure > pressure_cap || fuel_pressure < 0.0) {
        return {};
    }
    std::shared_ptr<bomb_data> bomb = std::make_shared<bomb_data>(mix_ratios, primer_ratios,
                   fuel_temp, fuel_pressure, thir_temp, mix_pressure, target_temp,
                   mix_gases, primer_gases,
                   tank);
    float stat = -1.f;
    // TODO critical
    // bool pre_met = restrictions_met(pre_restrictions);
    if (measure_before) stat = optstat_ref.get(*bomb);

    // simulate for up to tick_cap ticks
    size_t ticks = tank.tick_n(tick_cap);

    // TODO critical
    //if (!pre_met || !restrictions_met(post_restrictions)) {
    //    return {0.f, get_pressure(), false};
    //}
    bomb->set_state(std::move(tank), ticks);

    if (!measure_before) stat = optstat_ref.get(*bomb);

    bomb->optstat = stat;

    return opt_val_wrap(bomb);
}

} // namespace asim

template<>
inline std::string argp::type_sig<asim::field_ref<asim::bomb_data>> = "parameter";

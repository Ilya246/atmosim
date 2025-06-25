#include <algorithm>
#include <tuple>
#include <memory>

#include "sim.hpp"

namespace asim {

void bomb_data::sim_ticks(size_t up_to, field_ref<bomb_data> optstat_ref, bool measure_pre) {
    if (measure_pre) {
        fin_pressure = tank.mix.pressure();
        optstat = optstat_ref.get(*this);
    }

    size_t a_ticks = tank.tick_n(up_to);

    ticks = a_ticks;
    fin_pressure = tank.mix.pressure();
    fin_radius = gas_tank::calc_radius(fin_pressure);

    if (!measure_pre)
        optstat = optstat_ref.get(*this);
}

std::string bomb_data::mix_string(const std::vector<gas_ref>& gases, const std::vector<float>& fractions) const {
    std::string out;
    for (size_t i = 0; i < gases.size(); ++i) {
        out += std::format("{}% {}", fractions[i] * 100.f, gases[i].name());
        if (i != gases.size() - 1) out += " | ";
    }
    return out;
}

std::string bomb_data::mix_string_simple(const std::vector<gas_ref>& gases, const std::vector<float>& fractions) const {
    std::string out;
    for (size_t i = 0; i < gases.size(); ++i) {
        out += std::format("{}:{}", gases[i].name(), fractions[i]);
        if (i != gases.size() - 1) out += ",";
    }
    return out;
}

std::string bomb_data::print_very_simple() const {
    std::string out_str;
    std::vector<float> mix_fractions = get_fractions(mix_ratios);
    std::vector<float> primer_fractions = get_fractions(primer_ratios);
    // note: this format is supposed to be script-friendly and backwards-compatible
    out_str += std::format("os={} ti={} ft={} fp={} mp={} mt={} tt={} mi={} pm={}", optstat, ticks, fuel_temp, fuel_pressure, mix_to_pressure, mix_to_temp, thir_temp, mix_string_simple(mix_gases, mix_fractions), mix_string_simple(primer_gases, primer_fractions));
    return out_str;
}

std::string bomb_data::print_inline() const {
    std::string out_str;

    std::vector<float> mix_fractions = get_fractions(mix_ratios);
    std::vector<float> primer_fractions = get_fractions(primer_ratios);

    float required_primer_p = pressure_cap + (pressure_cap - fuel_pressure);

    out_str += std::format("S: [ time {:.1f}s | radius {:.2f}til | optstat {} ] ", ticks * tickrate, fin_radius, optstat);
    out_str += std::format("M: [ {} | {}K | {}kPa ] ", mix_string(mix_gases, mix_fractions), fuel_temp, fuel_pressure);
    out_str += std::format("C: [ {} | {}K | >{}kPa ]", mix_string(primer_gases, primer_fractions), thir_temp, required_primer_p);

    return out_str;
}

std::string bomb_data::print_full() const {
    std::string out_str;

    std::vector<float> mix_fractions = get_fractions(mix_ratios);
    std::vector<float> primer_fractions = get_fractions(primer_ratios);
    size_t mix_c = mix_gases.size(), primer_c = primer_gases.size(), total_c = mix_c + primer_gases.size();

    std::vector<std::pair<float, std::string>> min_amounts(mix_gases.size() + primer_gases.size());
    float required_volume = (required_transfer_volume + tank.mix.volume);
    for (size_t i = 0; i < mix_c; ++i) {
        min_amounts[i] = {to_mols(mix_fractions[i] * fuel_pressure, required_volume, fuel_temp), (std::string)mix_gases[i].name()};
    }
    float required_primer_p = pressure_cap + (pressure_cap - fuel_pressure);
    required_primer_p *= required_volume / required_transfer_volume;
    for (size_t i = 0; i < primer_c; ++i) {
        min_amounts[i + mix_c] = {to_mols(primer_fractions[i] * required_primer_p, required_volume, thir_temp), (std::string)primer_gases[i].name()};
    }
    std::string req_str;
    for (size_t i = 0; i < total_c; ++i) {
        req_str += std::format("{:.2f}mol {}", min_amounts[i].first, min_amounts[i].second);
        if (i + 1 != total_c) req_str += " | ";
    }

    out_str += std::format("STATS: [ time {:.1f}s | radius {:.2f}til | optstat {} ]\n", ticks * tickrate, fin_radius, optstat);
    out_str += std::format("MIX:   [ {} | {}K | {}kPa ]\n", mix_string(mix_gases, mix_fractions), fuel_temp, fuel_pressure);
    out_str += std::format("CAN:   [ {} | {}K | >{}kPa ]\n", mix_string(primer_gases, primer_fractions), thir_temp, required_primer_p);
    out_str += std::format("REQ:   [ {} ]", req_str);

    return out_str;
}

field_ref<bomb_data> bomb_data::radius_field(offsetof(bomb_data, fin_radius), field_ref<bomb_data>::float_f);
field_ref<bomb_data> bomb_data::ticks_field(offsetof(bomb_data, ticks), field_ref<bomb_data>::int_f);
field_ref<bomb_data> bomb_data::temperature_field(offsetof(bomb_data, tank.mix.temperature), field_ref<bomb_data>::float_f);

std::string params_supported_str = "radius, ticks, temperature";

std::istream& operator>>(std::istream& stream, field_ref<bomb_data>& re) {
    std::string val;
    stream >> val;
    if (val == "radius") re = bomb_data::radius_field;
    else if (val == "ticks") re = bomb_data::ticks_field;
    else if (val == "temperature") re = bomb_data::temperature_field;
    else stream.setstate(std::ios_base::failbit);
    return stream;
}

opt_val_wrap do_sim(const std::vector<float>& in_args, const std::tuple<const std::vector<gas_ref>&, const std::vector<gas_ref>&, bool, size_t, field_ref<bomb_data>, const std::vector<field_restriction<bomb_data>>&, const std::vector<field_restriction<bomb_data>>&>& args) {
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
    const std::vector<field_restriction<bomb_data>>& pre_restrictions = std::get<5>(args);
    const std::vector<field_restriction<bomb_data>>& post_restrictions = std::get<6>(args);

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
    gas_tank mix_tank;

    // specific heat is heat capacity of 1mol and fractions sum up to 1mol
    float fuel_specheat = get_mix_heat_capacity(mix_gases, mix_fractions);
    float primer_specheat = get_mix_heat_capacity(primer_gases, primer_fractions);
    // to how much we want to fill the tank
    float fuel_pressure = (target_temp / thir_temp - 1.f) * pressure_cap / (fuel_specheat / primer_specheat - 1.f + target_temp * (1.f / thir_temp - fuel_specheat / primer_specheat / fuel_temp));
    mix_tank.mix.canister_fill_to(mix_gases, mix_fractions, fuel_temp, fuel_pressure);
    mix_tank.mix.canister_fill_to(primer_gases, primer_fractions, thir_temp, pressure_cap);
    float mix_pressure = mix_tank.mix.pressure();

    // invalid mix, abort
    if (fuel_pressure > pressure_cap || fuel_pressure < 0.0) {
        return {};
    }

    std::shared_ptr<bomb_data> bomb = std::make_shared<bomb_data>(mix_ratios, primer_ratios,
                   fuel_temp, fuel_pressure, thir_temp, mix_pressure, target_temp,
                   mix_gases, primer_gases,
                   std::move(mix_tank));

    bool pre_met = std::none_of(pre_restrictions.begin(), pre_restrictions.end(), [&bomb](const auto& r){ return !r.OK(*bomb); });

    // simulate for up to tick_cap ticks
    bomb->sim_ticks(tick_cap, optstat_ref, measure_before);

    bool post_met = std::none_of(post_restrictions.begin(), post_restrictions.end(), [&bomb](const auto& r){ return !r.OK(*bomb); });
    return opt_val_wrap(bomb, pre_met && post_met);
}

}

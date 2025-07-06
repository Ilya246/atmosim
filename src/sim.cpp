#include <algorithm>
#include <cmath>
#include <memory>

#include "sim.hpp"
#include "constants.hpp"
#include "gas.hpp"
#include "utility.hpp"

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
        out += std::format("{}% {}", str_round_to(fractions[i] * 100.f, round_ratio_to * 100.f), gases[i].name());
        if (i != gases.size() - 1) out += " | ";
    }
    return out;
}

// [[gas1,frac1],[gas2,frac2],...]
std::string bomb_data::mix_string_simple(const std::vector<gas_ref>& gases, const std::vector<float>& fractions) const {
    std::string out = "[";
    for (size_t i = 0; i < gases.size(); ++i) {
        out += std::format("[{},{}]", gases[i].name(), fractions[i]);
        if (i != gases.size() - 1) out += ",";
    }
    return out + "]";
}

std::string bomb_data::print_very_simple() const {
    std::string out_str;
    std::vector<float> mix_fractions = get_fractions(mix_ratios);
    std::vector<float> primer_fractions = get_fractions(primer_ratios);
    // note: this format is supposed to be script-friendly and backwards-compatible
    out_str += std::format("os={} ti={} ft={} fp={} tp={} mt={} tt={} mi={} pm={}", optstat, ticks, fuel_temp, fuel_pressure, to_pressure, mix_to_temp, thir_temp, mix_string_simple(mix_gases, mix_fractions), mix_string_simple(primer_gases, primer_fractions));
    return out_str;
}

std::string bomb_data::serialize() const {
    std::string out_str;
    std::vector<float> mix_fractions = get_fractions(mix_ratios);
    std::vector<float> primer_fractions = get_fractions(primer_ratios);
    out_str += std::format("ft={} fp={} tp={} tt={} mi={} pm={}", fuel_temp, fuel_pressure, to_pressure, thir_temp, mix_string_simple(mix_gases, mix_fractions), mix_string_simple(primer_gases, primer_fractions));
    return out_str;
}

bomb_data bomb_data::deserialize(std::string_view str) {
    std::map<std::string, std::string> kv_pairs;
    size_t start = 0;
    // parse k=v into map
    while (start < str.size()) {
        size_t eq_pos = str.find('=', start);
        if (eq_pos == std::string_view::npos) break;
        std::string key(str.substr(start, eq_pos - start));
        size_t space_pos = str.find(' ', eq_pos);
        if (space_pos == std::string_view::npos) space_pos = str.size();
        std::string value(str.substr(eq_pos + 1, space_pos - eq_pos - 1));
        kv_pairs[key] = value;
        start = space_pos + 1;
    }

    float fuel_temp = argp::parse_value<float>(kv_pairs["ft"]);
    float fuel_pressure = argp::parse_value<float>(kv_pairs["fp"]);
    float to_pressure = argp::parse_value<float>(kv_pairs["tp"]);
    float thir_temp = argp::parse_value<float>(kv_pairs["tt"]);
    auto mix_gases = argp::parse_value<std::vector<std::pair<gas_ref, float>>>(kv_pairs["mi"]);
    auto primer_gases = argp::parse_value<std::vector<std::pair<gas_ref, float>>>(kv_pairs["pm"]);

    // try to reconstruct the tank
    gas_tank tank;
    tank.mix.canister_fill_to(mix_gases, fuel_temp, fuel_pressure);
    tank.mix.canister_fill_to(primer_gases, thir_temp, to_pressure);

    std::vector<float> mix_ratios, primer_ratios;
    std::vector<gas_ref> mix_refs, primer_refs;
    for (const auto& [k, v] : mix_gases) {
        mix_ratios.push_back(v);
        mix_refs.push_back(k);
    }
    for (const auto& [k, v] : primer_gases) {
        primer_ratios.push_back(v);
        primer_refs.push_back(k);
    }

    bomb_data data(
        mix_ratios,
        primer_ratios,
        to_pressure,
        fuel_temp,
        fuel_pressure,
        thir_temp,
        tank.mix.temperature,
        mix_refs,
        primer_refs,
        std::move(tank),
        false
    );
    return data;
}

// this is kinda cursed but if it works it works
std::string bomb_data::measure_tolerances(float min_ratio) const {
    const size_t measure_iters = 100;
    const float target_radius = fin_radius * min_ratio;
    const float target_ticks = ticks * min_ratio;
    std::string msg;

    auto test_variation = [&](auto&& adjust_fn) -> bool {
        bomb_data d_copy(*this);
        adjust_fn(d_copy);
        if (*std::min_element(d_copy.mix_ratios.begin(), d_copy.mix_ratios.end()) < 0.f) return false;
        if (*std::min_element(d_copy.primer_ratios.begin(), d_copy.primer_ratios.end()) < 0.f) return false;
        if (d_copy.fuel_temp < 0.f || d_copy.fuel_pressure < 0.f || d_copy.thir_temp < 0.f || d_copy.to_pressure < 0.f) return false;
        gas_tank tank;
        tank.mix.canister_fill_to(d_copy.mix_gases, get_fractions(d_copy.mix_ratios), d_copy.fuel_temp, d_copy.fuel_pressure);
        tank.mix.canister_fill_to(d_copy.primer_gases, get_fractions(d_copy.primer_ratios), d_copy.thir_temp, d_copy.to_pressure);
        size_t c_ticks = tank.tick_n(ticks / min_ratio);
        return tank.calc_radius() >= target_radius && c_ticks >= target_ticks;
    };

    auto find_tolerance = [&](auto&& adjust_fn, float start, float dir) -> float {
        float base = 0.f, adj = std::abs(start) / 1024.f;
        float farthest = start;
        bool had_invalid = false;
        // binary search
        for (size_t i = 0; i < measure_iters; ++i) {
            float test_val = start + (base + adj) * dir;
            bool valid = test_variation([&](bomb_data& c){ adjust_fn(c, test_val); });
            if (valid) {
                farthest = test_val;
                base = base + adj;
                adj *= had_invalid ? 0.5f : 2.f;
            } else {
                adj *= 0.5f;
                had_invalid = true;
            }
        }
        return farthest;
    };

    auto find_tolerances = [&](auto&& adjust_fn, float start) -> std::pair<float, float> {
        return {find_tolerance(adjust_fn, start, -1.f), find_tolerance(adjust_fn, start, 1.f)};
    };

    auto [ft_min, ft_max] = find_tolerances([](auto& c, float v){ c.fuel_temp = v; }, fuel_temp);
    msg += std::format("  Fuel temp: {}K - {}K\n", ft_min, ft_max);

    auto [fp_min, fp_max] = find_tolerances([](auto& c, float v){ c.fuel_pressure = v; }, fuel_pressure);
    msg += std::format("  Fuel pressure: {}kPa - {}kPa\n", fp_min, fp_max);

    auto [tt_min, tt_max] = find_tolerances([](auto& c, float v){ c.thir_temp = v; }, thir_temp);
    msg += std::format("  Primer temp: {}K - {}K\n", tt_min, tt_max);

    auto [tp_min, tp_max] = find_tolerances([](auto& c, float v){ c.to_pressure = v; }, to_pressure);
    msg += std::format("  Release pressure: {}kPa - {}kPa\n", tp_min, tp_max);

    if (mix_ratios.size() > 1) {
        float mix_sum = std::accumulate(mix_ratios.begin(), mix_ratios.end(), 0.f);
        for (size_t i = 0; i < mix_ratios.size(); ++i) {
            float orig_ratio = mix_ratios[i];
            auto [min_ratio, max_ratio] = find_tolerances([i](auto& c, float v){ c.mix_ratios[i] = v; }, orig_ratio);
            min_ratio /= mix_sum + min_ratio - orig_ratio;
            max_ratio /= mix_sum + max_ratio - orig_ratio;

            msg += std::format("  Mix {}: {}% - {}%\n", mix_gases[i].name(), min_ratio * 100.f, max_ratio * 100.f);
        }
    }

    if (primer_ratios.size() > 1) {
        float primer_sum = std::accumulate(primer_ratios.begin(), primer_ratios.end(), 0.f);
        for (size_t i = 0; i < primer_ratios.size(); ++i) {
            float orig_ratio = primer_ratios[i];
            auto [min_ratio, max_ratio] = find_tolerances([i](auto& c, float v){ c.primer_ratios[i] = v; }, orig_ratio);
            min_ratio /= primer_sum + min_ratio - orig_ratio;
            max_ratio /= primer_sum + max_ratio - orig_ratio;

            msg += std::format("  Primer {}: {}% - {}%\n", primer_gases[i].name(), min_ratio * 100.f, max_ratio * 100.f);
        }
    }

    return msg;
}

std::string bomb_data::print_inline() const {
    size_t pressure_round_digs = round_pressure_to < 1e-6f ? 6 : get_float_digits(round_pressure_to);
    size_t temp_round_digs = round_temp_to < 1e-6f ? 6 :get_float_digits(round_temp_to);
    std::string out_str;

    std::vector<float> mix_fractions = get_fractions(mix_ratios);
    std::vector<float> primer_fractions = get_fractions(primer_ratios);

    float required_primer_p = to_pressure + (to_pressure - fuel_pressure);

    out_str += std::format("S: [ time {:.1f}s | radius {:.2f}til | optstat {} ] ",
                           ticks * tickrate, fin_radius, optstat);
    out_str += std::format("M: [ {} | {:.{}f}K | {:.{}f}kPa ] ",
                           mix_string(mix_gases, mix_fractions), fuel_temp, temp_round_digs, fuel_pressure, pressure_round_digs);
    out_str += std::format("C: [ {} | {:.{}f}K | {:.{}f}kPa | >{}kPa ]",
                           mix_string(primer_gases, primer_fractions), thir_temp, temp_round_digs, to_pressure, pressure_round_digs, required_primer_p);

    return out_str;
}

std::string bomb_data::print_full() const {
    std::string out_str;
    size_t pressure_round_digs = round_pressure_to < 1e-6f ? 6 : get_float_digits(round_pressure_to);
    size_t temp_round_digs = round_temp_to < 1e-6f ? 6 :get_float_digits(round_temp_to);

    std::vector<float> mix_fractions = get_fractions(mix_ratios);
    std::vector<float> primer_fractions = get_fractions(primer_ratios);
    size_t mix_c = mix_gases.size(), primer_c = primer_gases.size(), total_c = mix_c + primer_gases.size();

    std::vector<std::pair<float, std::string>> min_amounts(mix_gases.size() + primer_gases.size());
    float required_volume = (required_transfer_volume + tank.mix.volume);
    for (size_t i = 0; i < mix_c; ++i) {
        min_amounts[i] = {to_mols(mix_fractions[i] * fuel_pressure, required_volume, fuel_temp), (std::string)mix_gases[i].name()};
    }
    float required_primer_p = to_pressure + (to_pressure - fuel_pressure);
    required_primer_p *= required_volume / required_transfer_volume;
    for (size_t i = 0; i < primer_c; ++i) {
        min_amounts[i + mix_c] = {to_mols(primer_fractions[i] * required_primer_p, required_volume, thir_temp), (std::string)primer_gases[i].name()};
    }
    std::string req_str;
    for (size_t i = 0; i < total_c; ++i) {
        req_str += std::format("{:.0f}mol {}", min_amounts[i].first, min_amounts[i].second);
        if (i + 1 != total_c) req_str += " | ";
    }

    out_str += std::format("STATS: [ time {:.1f}s | radius {:.2f}til | optstat {} ]\n",
                           ticks * tickrate, fin_radius, optstat);
    out_str += std::format("MIX:   [ {} | {:.{}f}K | {:.{}f}kPa ]\n",
                           mix_string(mix_gases, mix_fractions), fuel_temp, temp_round_digs, fuel_pressure, pressure_round_digs);
    out_str += std::format("CAN:   [ {} | {:.{}f}K | release {:.{}f}kPa | >{:.0f}kPa ]\n",
                           mix_string(primer_gases, primer_fractions), thir_temp, temp_round_digs, to_pressure, pressure_round_digs, required_primer_p);
    out_str += std::format("REQ:   [ {} ]", req_str);

    return out_str;
}

const field_ref<bomb_data> bomb_data::radius_field(offsetof(bomb_data, fin_radius), field_ref<bomb_data>::float_f);
const field_ref<bomb_data> bomb_data::ticks_field(offsetof(bomb_data, ticks), field_ref<bomb_data>::int_f);
const field_ref<bomb_data> bomb_data::temperature_field(offsetof(bomb_data, tank.mix.temperature), field_ref<bomb_data>::float_f);
const field_ref<bomb_data> bomb_data::integrity_field(offsetof(bomb_data, tank.integrity), field_ref<bomb_data>::int_f);
const std::map<gas_ref, field_ref<bomb_data>> bomb_data::gas_fields = []() {
    std::map<gas_ref, field_ref<bomb_data>> map;
    for (size_t i = 0; i < gas_count; ++i) {
        map[{i}] = {offsetof(bomb_data, tank.mix.amounts) + i * sizeof(float), field_ref<bomb_data>::float_f};
    }
    return map;
}();

// TODO: make this more sane somehow
std::string params_supported_str = "radius, ticks, temperature, integrity, " + list_gases();
std::istream& operator>>(std::istream& stream, field_ref<bomb_data>& re) {
    std::string val;
    stream >> val;
    if (val == "radius") re = bomb_data::radius_field;
    else if (val == "ticks") re = bomb_data::ticks_field;
    else if (val == "temperature") re = bomb_data::temperature_field;
    else if (val == "integrity") re = bomb_data::integrity_field;
    else if (is_valid_gas(val)) re = bomb_data::gas_fields.at(string_gas_map.at(val));
    else stream.setstate(std::ios_base::failbit);
    return stream;
}

opt_val_wrap do_sim(const std::vector<float>& in_args, const bomb_args& args) {
    // read input parameters
    float target_temp = in_args[0];
    float fuel_temp = in_args[1];
    float thir_temp = in_args[2];
    float fill_pressure = in_args[3];
    target_temp = round_to(target_temp, args.round_temp_to);
    fuel_temp = round_to(fuel_temp, args.round_temp_to);
    thir_temp = round_to(thir_temp, args.round_temp_to);
    // only round fill pressure if it's not too close to pressure cap
    if (std::abs(fill_pressure - pressure_cap) > args.round_pressure_to * 2.f) {
        fill_pressure = std::min(pressure_cap, round_to(fill_pressure, args.round_pressure_to));
    }
    // invalid mix, abort early
    if ((target_temp > fuel_temp) == (target_temp > thir_temp)) {
        return {};
    }
    const std::vector<gas_ref>& mix_gases = args.mix_gases;
    const std::vector<gas_ref>& primer_gases = args.primer_gases;
    bool measure_before = args.measure_before;
    size_t tick_cap = args.tick_cap;
    field_ref<bomb_data> optstat_ref = args.opt_param;
    const std::vector<field_restriction<bomb_data>>& pre_restrictions = args.pre_restrictions;
    const std::vector<field_restriction<bomb_data>>& post_restrictions = args.post_restrictions;

    // read gas ratios
    std::vector<float> mix_ratios(mix_gases.size(), 1.f);
    std::vector<float> primer_ratios(primer_gases.size(), 1.f);
    size_t mg_s = mix_gases.size() - 1;
    size_t pg_s = primer_gases.size() - 1;
    for (size_t i = 0; i < mg_s; ++i) {
        mix_ratios[i + 1] = std::exp(in_args[4 + i]);
    }
    for (size_t i = 0; i < pg_s; ++i) {
        primer_ratios[i + 1] = std::exp(in_args[4 + mg_s + i]);
    }

    std::vector<float> mix_fractions = get_fractions(mix_ratios);
    for (float& f : mix_fractions) f = round_to(f, args.round_ratio_to);
    mix_fractions *= 1.f / std::accumulate(mix_fractions.begin(), mix_fractions.end(), 0.f);
    std::vector<float> primer_fractions = get_fractions(primer_ratios);
    for (float& f : primer_fractions) f = round_to(f, args.round_ratio_to);
    primer_fractions *= 1.f / std::accumulate(primer_fractions.begin(), primer_fractions.end(), 0.f);

    // set up the tank
    gas_tank mix_tank;

    // specific heat is heat capacity of 1mol and fractions sum up to 1mol
    float fuel_specheat = get_mix_heat_capacity(mix_gases, mix_fractions);
    float primer_specheat = get_mix_heat_capacity(primer_gases, primer_fractions);
    // to how much we want to fill the tank
    float fuel_pressure = (target_temp / thir_temp - 1.f) * fill_pressure / (fuel_specheat / primer_specheat - 1.f + target_temp * (1.f / thir_temp - fuel_specheat / primer_specheat / fuel_temp));
    fuel_pressure = round_to(fuel_pressure, args.round_pressure_to);
    mix_tank.mix.canister_fill_to(mix_gases, mix_fractions, fuel_temp, fuel_pressure);
    mix_tank.mix.canister_fill_to(primer_gases, primer_fractions, thir_temp, fill_pressure);

    // invalid mix, abort
    if (fuel_pressure > fill_pressure || fuel_pressure < 0.0) {
        return {};
    }

    std::shared_ptr<bomb_data> bomb = std::make_shared<bomb_data>(mix_fractions, primer_fractions, fill_pressure,
                   fuel_temp, fuel_pressure, thir_temp, target_temp,
                   mix_gases, primer_gases,
                   std::move(mix_tank), args.round_pressure_to, args.round_temp_to, args.round_ratio_to);

    bool pre_met = std::none_of(pre_restrictions.begin(), pre_restrictions.end(), [&bomb](const auto& r){ return !r.OK(*bomb); });

    // simulate for up to tick_cap ticks
    bomb->sim_ticks(tick_cap, optstat_ref, measure_before);

    bool post_met = std::none_of(post_restrictions.begin(), post_restrictions.end(), [&bomb](const auto& r){ return !r.OK(*bomb); });
    return opt_val_wrap(bomb, pre_met && post_met);
}

}

#pragma once

#include <string>
#include <vector>

#include <argparse/read.hpp>

#include "gas.hpp"
#include "tank.hpp"
#include "utility.hpp"

namespace asim {

template<typename T>
struct field_ref {
    enum field_type {invalid_f, int_f, float_f};

    size_t offset = -1;
    field_type type = invalid_f;

    float get(const T& from) const {
        CHECKEXCEPT {
            if (offset == (size_t)-1) throw std::runtime_error("tried getting value of unset field reference");
        }
        switch (type) {
            case (float_f): return *(float*)((char*)&from + offset);
            case (int_f): return *(int*)((char*)&from + offset);
            default: throw std::runtime_error("tried getting value of invalid field reference");
        }
    }
};

template<typename T>
struct field_restriction {
    field_ref<T> field;
    float min_v, max_v;

    bool OK(const T& what) const {
        float val = field.get(what);
        return val >= min_v && val <= max_v;
    }
};

// read [name,min,max] format
template<typename T>
inline std::istream& operator>>(std::istream& stream, field_restriction<T>& re) {
    std::string all;
    std::getline(stream, all, '\0');
    std::string_view view(all);

    size_t cpos = view.find(',');
    size_t end_pos = view.find(argp::collection_close);
    if (cpos == std::string::npos || end_pos == std::string::npos) {
        stream.setstate(std::ios_base::failbit);
        return stream;
    }
    size_t cpos2 = view.find(',', cpos + 1);
    bool has_restr_b = cpos2 != std::string::npos;

    std::string_view param_s = view.substr(1, cpos - 1);
    re.field = argp::parse_value<field_ref<T>>(param_s);

    size_t restr_a_end = has_restr_b ? cpos2 : end_pos;
    std::string_view restr_a_str = view.substr(cpos + 1, restr_a_end - cpos - 1);
    // allow - as a shorthand for infinity
    float restr_a = restr_a_str == "-" ? -std::numeric_limits<float>::max() : argp::parse_value<float>(restr_a_str);

    float restr_b = std::numeric_limits<float>::max();
    if (has_restr_b) {
        std::string_view restr_b_str = view.substr(cpos2 + 1, view.size() - cpos2 - 2);
        restr_b = restr_b_str == "-" ? restr_b : argp::parse_value<float>(restr_b_str);
    }

    re.min_v = restr_a;
    re.max_v = restr_b;

    return stream;
}

extern std::string params_supported_str;

struct bomb_data {
    std::vector<float> mix_ratios, primer_ratios;
    float fuel_temp, fuel_pressure, thir_temp, mix_to_pressure, mix_to_temp;
    std::vector<gas_ref> mix_gases, primer_gases;
    gas_tank tank;
    float optstat;
    float fin_pressure, fin_radius = 0.f;
    int ticks;

    // TODO: make this more sane somehow?
    bomb_data(std::vector<float> mix_ratios, std::vector<float> primer_ratios,
              float fuel_temp, float fuel_pressure, float thir_temp, float mix_to_pressure, float mix_to_temp,
              const std::vector<gas_ref>& mix_gases, const std::vector<gas_ref>& primer_gases,
              gas_tank tank, float optstat = -1.f,
              int ticks = 0)
    :
        mix_ratios(mix_ratios), primer_ratios(primer_ratios),
        fuel_temp(fuel_temp), fuel_pressure(fuel_pressure), thir_temp(thir_temp), mix_to_pressure(mix_to_pressure), mix_to_temp(mix_to_temp),
        mix_gases(mix_gases), primer_gases(primer_gases),
        tank(tank), optstat(optstat),
        ticks(ticks) {};

    void sim_ticks(size_t up_to, field_ref<bomb_data> optstat_ref, bool measure_pre);

    std::string mix_string(const std::vector<gas_ref>& gases, const std::vector<float>& fractions) const;
    std::string mix_string_simple(const std::vector<gas_ref>& gases, const std::vector<float>& fractions) const;
    std::string print_very_simple() const;
    std::string print_inline() const;
    std::string print_full() const;

    static field_ref<bomb_data> radius_field;
    static field_ref<bomb_data> ticks_field;
    static field_ref<bomb_data> temperature_field;
};

std::istream& operator>>(std::istream& stream, field_ref<bomb_data>& re);

// wrapper for bomb_data for use by the optimiser
struct opt_val_wrap {
    std::shared_ptr<bomb_data> data = nullptr;
    bool valid_v = true;

    opt_val_wrap(): valid_v(false) {}
    opt_val_wrap(std::shared_ptr<bomb_data>& d): data(d), valid_v(d != nullptr) {}
    opt_val_wrap(std::shared_ptr<bomb_data>& d, bool val): data(d), valid_v(val) {}
    // methods below required for optimiser
    bool valid() const {
        return valid_v;
    }
    std::string rating() const {
        if (!data) return "[INVALID BOMB]";
        return data->print_inline();
    }
    bool operator>(const opt_val_wrap& rhs) const {
        return data->optstat == rhs.data->optstat ? data->fin_radius > rhs.data->fin_radius : data->optstat > rhs.data->optstat;
    }
    bool operator>=(const opt_val_wrap& rhs) const {
        return data->optstat >= rhs.data->optstat;
    }
    bool operator==(const opt_val_wrap& rhs) const {
        return data->optstat == rhs.data->optstat;
    }
};

// args: target_temp, fuel_temp, thir_temp, mix ratios..., primer ratios...
opt_val_wrap do_sim(const std::vector<float>& in_args, std::tuple<const std::vector<gas_ref>&, const std::vector<gas_ref>&, bool, size_t, field_ref<bomb_data>, const std::vector<field_restriction<bomb_data>>&, const std::vector<field_restriction<bomb_data>>&> args);

}

template<>
inline std::string argp::type_sig<asim::field_ref<asim::bomb_data>> = "parameter";

// make argp treat restriction as a container for [] syntax
template<typename K>
struct argp::is_container<asim::field_restriction<K>> : std::true_type {};

template<typename K>
inline const std::string argp::type_sig<asim::field_restriction<K>> = argp::collection_open + argp::type_sig<asim::field_ref<K>> + ",float,float" + argp::collection_close;

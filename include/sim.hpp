#include <string>
#include <vector>

#include <argparse/read.hpp>

#include "gas.hpp"
#include "tank.hpp"
#include "utility.hpp"

namespace asim {

struct bomb_data {
    std::vector<float> mix_ratios, primer_ratios;
    float fuel_temp, fuel_pressure, thir_temp, mix_to_pressure, mix_to_temp;
    std::vector<gas_ref> mix_gases, primer_gases;
    gas_tank tank;
    float optstat;
    float fin_pressure, fin_radius;
    int ticks;

    // TODO: make this more sane somehow?
    bomb_data(std::vector<float> mix_ratios, std::vector<float> primer_ratios,
              float fuel_temp, float fuel_pressure, float thir_temp, float mix_to_pressure, float mix_to_temp,
              const std::vector<gas_ref>& mix_gases, const std::vector<gas_ref>& primer_gases,
              gas_tank tank, float optstat = -1.f,
              int ticks = -1)
    :
        mix_ratios(mix_ratios), primer_ratios(primer_ratios),
        fuel_temp(fuel_temp), fuel_pressure(fuel_pressure), thir_temp(thir_temp), mix_to_pressure(mix_to_pressure), mix_to_temp(mix_to_temp),
        mix_gases(mix_gases), primer_gases(primer_gases),
        tank(tank), optstat(optstat),
        ticks(ticks) {};

    void set_state(gas_tank fin_tank, int i_ticks) {
        tank = fin_tank;
        ticks = i_ticks;
        fin_pressure = tank.mix.pressure();
        fin_radius = gas_tank::calc_radius(fin_pressure);
    }

    std::string mix_string(const std::vector<gas_ref>& gases, const std::vector<float>& fractions) const {
        std::string out;
        for (size_t i = 0; i < gases.size(); ++i) {
            out += std::format("{}% {}", fractions[i] * 100.f, gases[i].name());
            if (i != gases.size() - 1) out += " | ";
        }
        return out;
    }

    std::string mix_string_simple(const std::vector<gas_ref>& gases, const std::vector<float>& fractions) const {
        std::string out;
        for (size_t i = 0; i < gases.size(); ++i) {
            out += std::format("{}:{}", gases[i].name(), fractions[i]);
            if (i != gases.size() - 1) out += ",";
        }
        return out;
    }

    std::string print_very_simple() const {
        std::string out_str;
        std::vector<float> mix_fractions = get_fractions(mix_ratios);
        std::vector<float> primer_fractions = get_fractions(primer_ratios);
        // note: this format is supposed to be script-friendly and backwards-compatible
        out_str += std::format("os={} ti={} ft={} fp={} mp={} mt={} tt={} mi={} pm={}", optstat, ticks, fuel_temp, fuel_pressure, mix_to_pressure, mix_to_temp, thir_temp, mix_string_simple(mix_gases, mix_fractions), mix_string_simple(primer_gases, primer_fractions));
        return out_str;
    }

    std::string print_inline() const {
        std::string out_str;

        std::vector<float> mix_fractions = get_fractions(mix_ratios);
        std::vector<float> primer_fractions = get_fractions(primer_ratios);

        float required_primer_p = pressure_cap + (pressure_cap - fuel_pressure);

        out_str += std::format("S: [ time {:.1f}s | radius {:.2f}til | optstat {} ] ", ticks * tickrate, fin_radius, optstat);
        out_str += std::format("M: [ {} | {}K | {}kPa ] ", mix_string(mix_gases, mix_fractions), fuel_temp, fuel_pressure);
        out_str += std::format("C: [ {} | {}K | >{}kPa ]", mix_string(primer_gases, primer_fractions), thir_temp, required_primer_p);

        return out_str;
    }

    std::string print_full() const {
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
};

// wrapper for bomb_data for use by the optimiser
struct opt_val_wrap {
    std::shared_ptr<bomb_data> data = nullptr;
    bool valid_v = true;

    opt_val_wrap(): valid_v(false) {}
    opt_val_wrap(std::shared_ptr<bomb_data>& d): data(d), valid_v(d != nullptr) {}

    static const opt_val_wrap worst(bool) {
        return {};
    }

    bool valid() const {
        return valid_v;
    }

    bool operator>(const opt_val_wrap& rhs) const {
        return data->optstat == rhs.data->optstat ? data->fin_pressure > rhs.data->fin_pressure : data->optstat > rhs.data->optstat;
    }

    bool operator>=(const opt_val_wrap& rhs) const {
        return data->optstat >= rhs.data->optstat;
    }

    bool operator==(const opt_val_wrap& rhs) const {
        return data->optstat == rhs.data->optstat;
    }
};

template<typename T>
struct field_ref {
    enum field_type {invalid_f, int_f, float_f};

    size_t offset = -1;
    field_type type = invalid_f;

    float get(T& from) {
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

extern std::string params_supported_str;

std::istream& operator>>(std::istream& stream, field_ref<bomb_data>& re);

// args: target_temp, fuel_temp, thir_temp, mix ratios..., primer ratios...
opt_val_wrap do_sim(const std::vector<float>& in_args, std::tuple<const std::vector<gas_ref>&, const std::vector<gas_ref>&, bool, size_t, field_ref<bomb_data>> args);

}

template<>
inline std::string argp::type_sig<asim::field_ref<asim::bomb_data>> = "parameter";


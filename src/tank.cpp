#include <cmath>

#include "tank.hpp"

namespace asim {

float gas_tank::calc_radius() {
    return calc_radius(mix.pressure());
}

float gas_tank::calc_radius(float pressure) {
    if (pressure < tank_fragment_pressure) return 0.f;
    return std::sqrt((pressure - tank_fragment_pressure) / tank_fragment_scale);
}

// do one reaction tick and check state
std::pair<bool, float> gas_tank::_tick() {
    mix.reaction_tick();

    float pressure = mix.pressure();
    if (pressure > tank_fragment_pressure) {
        for (int i = 0; i < 3; ++i) {
            mix.reaction_tick();
        }
        state = st_exploded;
        return {false, pressure};
    }
    if (pressure > tank_rupture_pressure) {
        if (integrity <= 0) {
            state = st_ruptured;
            return {false, pressure};
        }
        --integrity;
        return {true, pressure};
    }
    if (pressure > tank_leak_pressure) {
        if (integrity <= 0) {
            for (float& amt : mix.amounts) {
                amt *= 0.75;
            }
        } else {
            --integrity;
        }
        return {true, pressure};
    }
    if (integrity < 3) {
        ++integrity;
    }
    return {true, pressure};
}

bool gas_tank::tick() {
    return _tick().first;
}

size_t gas_tank::tick_n(size_t ticks_limit) {
    float pre_pressure = mix.pressure();
    for (size_t i = 0; i < ticks_limit; ++i) {
        std::pair<bool, float> res = _tick();
        // early exit if we ruptured or if we're inert
        if (!res.first || pre_pressure == res.second) return i + 1;
        pre_pressure = res.second;
    }
    return ticks_limit;
}

std::string gas_tank::get_status() {
    return std::format("pressure {} temperature {} integ {} gases [{}]",
                        mix.pressure(), mix.temperature, integrity, mix.to_string());
}

}

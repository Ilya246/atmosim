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

bool gas_tank::tick() {
    mix.reaction_tick();

    float pressure = mix.pressure();
    if (pressure > tank_leak_pressure) {
        if (pressure > tank_rupture_pressure) {
            if (pressure > tank_fragment_pressure) {
                for (int i = 0; i < 3; ++i) {
                    mix.reaction_tick();
                }
                state = st_exploded;
                return false;
            }
            if (integrity <= 0) {
                state = st_ruptured;
                return false;
            }
            integrity--;
            return true;
        }
        if (integrity <= 0) {
            for (float& amt : mix.amounts) {
                amt *= 0.75;
            }
        } else {
            integrity--;
        }
        return true;
    }
    if (integrity < 3) {
        integrity++;
    }
    return true;
}

size_t gas_tank::tick_n(size_t ticks_limit) {
    for (size_t i = 0; i < ticks_limit; ++i) {
        if (!tick()) return i + 1;
    }
    return ticks_limit;
}

}

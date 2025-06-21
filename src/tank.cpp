#include <cmath>

#include "tank.hpp"

float gas_tank::calc_radius(float pressure) {
    return std::sqrt((pressure - tank_fragment_pressure) / tank_fragment_scale);
}

bool gas_tank::tick() {
    float pressure = mix.pressure();
    if (pressure > tank_leak_pressure) {
        if (pressure > tank_rupture_pressure) {
            if (pressure > tank_fragment_pressure) {
                for (int i = 0; i < 3; ++i) {
                    mix.reaction_tick();
                }
                state = exploded;
                radius_cache = calc_radius(mix.pressure());
                return false;
            }
            if (integrity <= 0) {
                state = ruptured;
                radius_cache = 0.0;
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

bool gas_tank::tick_n(size_t ticks_limit) {
    for (size_t i = 0; i < ticks_limit; ++i) {
        if (!tick()) return false;
    }
    return true;
}

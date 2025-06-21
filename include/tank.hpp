#pragma once

#include "constants.hpp"
#include "gas.hpp"

struct gas_tank {
    enum tank_state {
        intact = 0,
        ruptured = 1,
        exploded = 2
    };

    gas_mixture mix = gas_mixture(tank_volume);
    tank_state state;
    int integrity = 3;

    // go forward in time one tick
    // returns: whether we're still intact
    bool tick();
    // go forward up to ticks_limit ticks
    bool tick_n(size_t ticks_limit);

    float radius_cache = 0.f;
    static float calc_radius(float pressure);

    std::string get_status();
};

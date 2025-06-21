#pragma once

#include "constants.hpp"
#include "gas.hpp"

struct gas_tank {
    enum tank_state {
        st_intact = 0,
        st_ruptured = 1,
        st_exploded = 2
    };

    gas_mixture mix = gas_mixture(tank_volume);
    tank_state state = st_intact;
    int integrity = 3;

    // go forward in time one tick
    // returns: whether we're still intact
    bool tick();
    // go forward up to ticks_limit ticks
    // returns: how many ticks we went forward
    size_t tick_n(size_t ticks_limit);

    void fill_with(gas_ref gas, float temperature);
    void fill_with(gas_ref gas);

    float radius_cache = 0.f;
    static float calc_radius(float pressure);

    std::string get_status();
};

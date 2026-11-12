#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "gas.hpp"
#include "tank.hpp"

using Catch::Approx;
using namespace asim;

// UP TO DATE AS OF: 12.11.2026 (Goob ba5f781)
// Expected amounts come from TritiumFireReaction.cs.
// Heat scale cancels out, so these tests use the original energy values.
TEST_CASE("Goob tritium limiting reactants and energy", "[goob]") {
    REQUIRE(tritium_burn_fuel_ratio == 2.f);
    REQUIRE(fire_hydrogen_energy_released / heat_scale == Approx(2840000.f));

    struct reaction_case {
        const char* name;
        float trit, oxy, temp, burned, energy_per_mole;
    };
    const reaction_case cases[] {
        {"tritium limited", 10.f, 100.f, 1000.f, 1.f, 28400000.f},
        {"oxygen limited high energy", 10.f, 15.f, 1000.f, 0.75f, 28400000.f},
        {"limiting reactant boundary", 10.f, 20.f, 1000.f, 1.f, 28400000.f},
        {"oxygen poor", 10.f, 5.f, 1000.f, 0.05f, 2840000.f},
        {"below energy threshold", 1.f, 2.f, 400.f, 0.02f, 2840000.f},
    };
    for (const auto& c : cases) {
        DYNAMIC_SECTION(c.name) {
            gas_mixture mix(tank_volume);
            mix.set_amount_of(tritium, c.trit);
            mix.set_amount_of(oxygen, c.oxy);
            mix.temperature = c.temp;

            REQUIRE(mix.reaction_tick());
            const float final_trit = c.trit - c.burned;
            const float final_oxy = c.oxy - c.burned / 2.f;
            const float final_capacity = 10.f * final_trit + 20.f * final_oxy + 40.f * c.burned;
            const float expected_temp = (c.temp * (10.f * c.trit + 20.f * c.oxy)
                                        + c.energy_per_mole * c.burned) / final_capacity;
            REQUIRE(mix.amount_of(tritium) == Approx(final_trit));
            REQUIRE(mix.amount_of(oxygen) == Approx(final_oxy));
            REQUIRE(mix.amount_of(water_vapour) == Approx(c.burned));
            REQUIRE(mix.temperature == Approx(expected_temp));
        }
    }
}

TEST_CASE("Cooling-only tanks keep simulating", "[goob]") {
    gas_tank initial;
    initial.mix.set_amount_of(nitrogen, 1.f);
    initial.mix.set_amount_of(frezon, 0.2f);
    initial.mix.temperature = 300.f;

    gas_tank stepped = initial;
    for (int i = 0; i < 3; ++i) {
        const float before = stepped.mix.temperature;
        REQUIRE(stepped.tick());
        REQUIRE(stepped.mix.temperature < before);
        REQUIRE(stepped.state == gas_tank::st_intact);
    }

    gas_tank batched = initial;
    REQUIRE(batched.tick_n(3) == 3);
    REQUIRE(batched.mix.temperature == Approx(stepped.mix.temperature));
    REQUIRE(batched.mix.amount_of(frezon) == Approx(stepped.mix.amount_of(frezon)));
    REQUIRE(batched.mix.amount_of(nitrous_oxide) == Approx(stepped.mix.amount_of(nitrous_oxide)));
}

TEST_CASE("Reaction temperature boundaries", "[goob]") {
    SECTION("Frezon production includes its maximum temperature") {
        gas_mixture mix(tank_volume);
        mix.set_amount_of(oxygen, 50.f);
        mix.set_amount_of(tritium, 1.f);
        mix.set_amount_of(nitrogen, 10.f);
        mix.temperature = frezon_production_temp;
        gas_mixture above = mix;
        above.temperature = std::nextafter(mix.temperature, INFINITY);
        REQUIRE_FALSE(above.reaction_tick());
        REQUIRE(above.amount_of(tritium) == 1.f);
        REQUIRE(mix.reaction_tick());
        REQUIRE(mix.amount_of(tritium) == Approx(0.98f));
        REQUIRE(mix.amount_of(frezon) > 0.f);
    }
    SECTION("Nitrium decomposition includes its maximum temperature") {
        gas_mixture mix(tank_volume);
        mix.set_amount_of(oxygen, 1.f);
        mix.set_amount_of(nitrium, 1.f);
        mix.temperature = nitrium_decomp_temp;
        gas_mixture above = mix;
        above.temperature = std::nextafter(mix.temperature, INFINITY);
        REQUIRE_FALSE(above.reaction_tick());
        REQUIRE(mix.reaction_tick());
        REQUIRE(mix.amount_of(nitrium) == Approx(1.f - nitrium_decomp_temp / 2984.f));
    }
    SECTION("Frezon coolant is inert at its lower temperature") {
        gas_mixture mix(tank_volume);
        mix.set_amount_of(frezon, 1.f);
        mix.set_amount_of(nitrogen, 5.f);
        mix.temperature = frezon_cool_lower_temperature;
        REQUIRE_FALSE(mix.reaction_tick());
        REQUIRE(mix.temperature == frezon_cool_lower_temperature);
        REQUIRE(mix.amount_of(frezon) == 1.f);
    }
}

TEST_CASE("Prototype temperature gates use the start of the pass", "[goob]") {
    gas_mixture mix(tank_volume);
    mix.set_amount_of(oxygen, 10.f);
    mix.set_amount_of(plasma, 1.f);
    mix.set_amount_of(nitrium, 1.f);
    mix.temperature = 340.f;
    REQUIRE(mix.reaction_tick());
    REQUIRE(mix.temperature > plasma_fire_temp);
    REQUIRE(mix.amount_of(plasma) == 1.f);
    REQUIRE(mix.reaction_tick());
    REQUIRE(mix.amount_of(plasma) < 1.f);
}

TEST_CASE("Fragmentation performs three extra reaction passes", "[goob]") {
    gas_tank tank;
    tank.mix.set_amount_of(oxygen, 100.f);
    tank.mix.set_amount_of(tritium, 10.f);
    tank.mix.temperature = 1000.f;
    gas_mixture expected = tank.mix;
    REQUIRE(expected.reaction_tick());
    REQUIRE(expected.pressure() > tank_fragment_pressure);
    for (int i = 0; i < 3; ++i)
        REQUIRE(expected.reaction_tick());

    REQUIRE(tank.tick_n(10) == 1);
    REQUIRE(tank.state == gas_tank::st_exploded);
    REQUIRE(tank.mix.pressure() == Approx(expected.pressure()));
    REQUIRE(tank.mix.amount_of(tritium) == Approx(6.561f));
    REQUIRE(tank.calc_radius() > 26.f); // We report radius without the game's cap.
}

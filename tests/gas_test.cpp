#include <cmath>

#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "gas.hpp"

using Catch::Approx;

TEST_CASE("Gas System Fundamentals") {
    SECTION("Gas index mapping") {
        REQUIRE(gas_count == 9);
        REQUIRE(string_gas_map.size() == gas_count);
        REQUIRE(oxygen.name() == "oxygen");
        REQUIRE(nitrogen.name() == "nitrogen");
        REQUIRE(plasma.name() == "plasma");
        REQUIRE(tritium.name() == "tritium");
        REQUIRE(water_vapour.name() == "water_vapour");
        REQUIRE(carbon_dioxide.name() == "carbon_dioxide");
        REQUIRE(frezon.name() == "frezon");
        REQUIRE(nitrous_oxide.name() == "nitrous_oxide");
        REQUIRE(nitrium.name() == "nitrium");
    }

    SECTION("Utility functions") {
        // Test ideal gas law relationships
        const float pressure = 101.325f;
        const float volume = 1.0f;
        const float temp = 273.15f;
        const float mols = pressure * volume / R / temp;

        REQUIRE(mols == Approx(pressure * volume / (R * temp)).epsilon(0.001));
        REQUIRE(to_pressure(volume, mols, temp) == Approx(pressure).epsilon(0.001));
        REQUIRE(to_volume(pressure, mols, temp) == Approx(volume).epsilon(0.001));

        // Test temperature mixing with known ratios
        const float mix_temp = to_mix_temp(2.0f, 1.0f, 300.0f, 1.0f, 1.0f, 400.0f);
        REQUIRE(mix_temp == Approx((2.0f*1.0f*300.0f + 1.0f*1.0f*400.0f) / (2.0f*1.0f + 1.0f*1.0f)));
    }
}

TEST_CASE("Gas Reactions") {
    gas_mixture mix(1.0f);

    // UP TO DATE AS OF: 21.06.2025
    SECTION("Plasma fire reaction") {
        mix.adjust_amount_of(oxygen, 10.0f);
        mix.adjust_amount_of(plasma, 5.0f);
        mix.temperature = 2000.0f;

        mix.reaction_tick();

        REQUIRE(mix.amount_of(plasma) < 5.0f);
        REQUIRE(mix.amount_of(oxygen) < 10.0f);
        REQUIRE(mix.amount_of(carbon_dioxide) > 0.0f);
        REQUIRE(mix.amount_of(tritium) == 0.0f);
        REQUIRE(mix.temperature > 2000.0f);
    }

    // UP TO DATE AS OF: 21.06.2025
    SECTION("Plasma fire reaction with tritium production") {
        mix.adjust_amount_of(oxygen, 10.0f);
        mix.adjust_amount_of(plasma, 0.1f);
        mix.temperature = 2000.0f;

        mix.reaction_tick();

        REQUIRE(mix.amount_of(plasma) < 0.1f);
        REQUIRE(mix.amount_of(oxygen) < 10.0f);
        REQUIRE(mix.amount_of(carbon_dioxide) == 0.0f);
        REQUIRE(mix.amount_of(tritium) > 0.0f);
        REQUIRE(mix.temperature > 2000.0f);
    }

    // UP TO DATE AS OF: 21.06.2025
    SECTION("Tritium fire reaction") {
        mix.adjust_amount_of(oxygen, 20.0f);
        mix.adjust_amount_of(tritium, 2.0f);
        mix.temperature = 400.0f;

        mix.reaction_tick();

        REQUIRE(mix.amount_of(tritium) < 2.0f);
        REQUIRE(mix.amount_of(oxygen) < 20.0f);
        REQUIRE(mix.amount_of(water_vapour) > 0.0f);
        REQUIRE(mix.temperature > 400.0f);
    }

    // UP TO DATE AS OF: 21.06.2025
    SECTION("N2O decomposition") {
        mix.adjust_amount_of(nitrous_oxide, 10.0f);
        mix.temperature = 900.0f;

        mix.reaction_tick();

        REQUIRE(mix.amount_of(nitrous_oxide) == Approx(5.0f));
        REQUIRE(mix.amount_of(nitrogen) == Approx(5.0f));
        REQUIRE(mix.amount_of(oxygen) == Approx(2.5f));
    }

    // UP TO DATE AS OF: 21.06.2025
    SECTION("Frezon coolant reaction") {
        mix.adjust_amount_of(frezon, 10.0f);
        mix.adjust_amount_of(nitrogen, 20.0f);
        mix.temperature = 500.0f;

        mix.reaction_tick();

        REQUIRE(mix.amount_of(frezon) < 10.0f);
        REQUIRE(mix.amount_of(nitrogen) < 20.0f);
        REQUIRE(mix.amount_of(nitrous_oxide) > 0.0f);
        REQUIRE(mix.temperature < 500.0f);
    }

    // UP TO DATE AS OF: 21.06.2025
    SECTION("Nitrium decomposition") {
        mix.adjust_amount_of(nitrium, 1.0f);
        mix.adjust_amount_of(oxygen, 1.0f);
        mix.temperature = 200.0f;

        mix.reaction_tick();

        REQUIRE(mix.amount_of(nitrium) < 1.0f);
        REQUIRE(mix.amount_of(water_vapour) > 0.0f);
        REQUIRE(mix.amount_of(nitrogen) > 0.0f);
        REQUIRE(mix.temperature > 200.0f);
    }
}

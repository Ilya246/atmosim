#include <catch2/catch_test_macros.hpp>
#include "../src/gas.cpp"
#include <cmath>

using namespace Catch::literals;

TEST_CASE("Gas System Fundamentals") {
    SECTION("Gas index mapping") {
        REQUIRE(gas_count == 9);
        REQUIRE(string_gas_map.size() == gas_count);
        REQUIRE(oxygen_idx == 0);
        REQUIRE(nitrogen_idx == 1);
        REQUIRE(plasma_idx == 2);
        REQUIRE(tritium_idx == 3);
        REQUIRE(water_vapour_idx == 4);
        REQUIRE(carbon_dioxide_idx == 5);
        REQUIRE(frezon_idx == 6);
        REQUIRE(nitrous_oxide_idx == 7);
        REQUIRE(nitrium_idx == 8);
    }

    SECTION("Utility functions") {
        REQUIRE(to_mols(101.325f, 1.0f, 273.15f) == Approx(0.044615f));
        REQUIRE(to_pressure(1.0f, 1.0f, 273.15f) == Approx(2271.03f).epsilon(0.01));
        REQUIRE(to_volume(101.325f, 1.0f, 273.15f) == Approx(22.414f).epsilon(0.01));
        REQUIRE(to_mix_temp(2.0f, 1.0f, 300.0f, 1.0f, 1.0f, 400.0f) == Approx(333.333f));
    }
}

TEST_CASE("Gas Reactions") {
    gas_mixture mix(1.0f); // 1m³ volume

    SECTION("Plasma fire reaction") {
        mix.update_amount_of(oxygen, 10.0f);
        mix.update_amount_of(plasma, 5.0f);
        mix.temperature = 2000.0f;
        
        mix.reaction_tick();
        
        REQUIRE(mix.amount_of(plasma) < 5.0f);
        REQUIRE(mix.amount_of(oxygen) < 10.0f);
        REQUIRE(mix.amount_of(carbon_dioxide) > 0.0f);
        REQUIRE(mix.amount_of(tritium) > 0.0f);
        REQUIRE(mix.temperature > 2000.0f);
    }

    SECTION("Tritium fire reaction") {
        mix.update_amount_of(oxygen, 20.0f);
        mix.update_amount_of(tritium, 2.0f);
        mix.temperature = 400.0f;
        
        mix.reaction_tick();
        
        REQUIRE(mix.amount_of(tritium) < 2.0f);
        REQUIRE(mix.amount_of(oxygen) < 20.0f);
        REQUIRE(mix.amount_of(water_vapour) > 0.0f);
        REQUIRE(mix.temperature > 400.0f);
    }

    SECTION("N2O decomposition") {
        mix.update_amount_of(nitrous_oxide, 10.0f);
        mix.temperature = 900.0f;
        
        mix.reaction_tick();
        
        REQUIRE(mix.amount_of(nitrous_oxide) == Approx(5.0f));
        REQUIRE(mix.amount_of(nitrogen) == Approx(5.0f));
        REQUIRE(mix.amount_of(oxygen) == Approx(2.5f));
    }

    SECTION("Frezon coolant reaction") {
        mix.update_amount_of(frezon, 10.0f);
        mix.update_amount_of(nitrogen, 20.0f);
        mix.temperature = 500.0f;
        
        mix.reaction_tick();
        
        REQUIRE(mix.amount_of(frezon) < 10.0f);
        REQUIRE(mix.amount_of(nitrogen) < 20.0f);
        REQUIRE(mix.amount_of(nitrous_oxide) > 0.0f);
        REQUIRE(mix.temperature < 500.0f);
    }

    SECTION("Nitrium decomposition") {
        mix.update_amount_of(nitrium, 1.0f);
        mix.update_amount_of(oxygen, 1.0f);
        mix.temperature = 200.0f;
        
        mix.reaction_tick();
        
        REQUIRE(mix.amount_of(nitrium) < 1.0f);
        REQUIRE(mix.amount_of(water_vapour) > 0.0f);
        REQUIRE(mix.amount_of(nitrogen) > 0.0f);
        REQUIRE(mix.temperature > 200.0f);
    }
}

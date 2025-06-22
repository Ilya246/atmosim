#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <argparse/args.hpp>

#include "gas.hpp"
#include "tank.hpp"

using Catch::Approx;

TEST_CASE("Gas system fundamentals") {
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

TEST_CASE("Gas system argparse test") {
    SECTION("Gas read") {
        gas_ref read_gas1, read_gas2;
        std::vector<gas_ref> gas_vec;

        std::vector<std::shared_ptr<argp::base_argument>> args = {
            argp::make_argument("gas", "", "", read_gas1),
            argp::make_argument("gas2", "g2", "", read_gas2),
            argp::make_argument("gases", "", "", gas_vec)
        };

        char* argv[] = {(char*)"./atmosim", (char*)"--gas=tritium", (char*)"-g2=nitrous_oxide", (char*)"--gases=[oxygen,plasma,water_vapour]"};
        argp::parse_arguments(args, 4, argv);

        REQUIRE(read_gas1 == tritium);
        REQUIRE(read_gas2 == nitrous_oxide);
        REQUIRE(gas_vec.size() == 3);
        REQUIRE(gas_vec[0] == oxygen);
        REQUIRE(gas_vec[1] == plasma);
        REQUIRE(gas_vec[2] == water_vapour);
    }
}

TEST_CASE("Gas reactions") {
    gas_mixture mix(tank_volume);

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
        mix.temperature = 12000.0f;

        mix.reaction_tick();

        REQUIRE(mix.amount_of(tritium) < 2.0f);
        REQUIRE(mix.amount_of(oxygen) < 20.0f);
        REQUIRE(mix.amount_of(water_vapour) > 0.0f);
        REQUIRE(mix.temperature > 12000.0f);
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

TEST_CASE("Tank simulation validation") {
    gas_tank tank;

    // UP TO DATE AS OF: 21.06.2025
    SECTION("ticks-13r-22.5s-PT+O") {
        float mix_pressure = 684.853f;
        float mix_temp = 382.42734f;
        float plasma_frac = 0.52208485f;
        float tritium_frac = 1.f - plasma_frac;
        float radius_expected = 13.02f;
        size_t ticks_expected = 45;

        tank.mix.temperature = mix_temp;
        tank.mix.adjust_pressure_of(plasma, mix_pressure * plasma_frac);
        tank.mix.adjust_pressure_of(tritium, mix_pressure * tritium_frac);
        tank.mix.canister_fill_to(oxygen, T20C, pressure_cap);

        size_t ticks = tank.tick_n(ticks_expected);

        REQUIRE(tank.state == tank.st_exploded);
        REQUIRE(tank.radius_cache == Catch::Approx(radius_expected).epsilon(0.01f));
        REQUIRE(ticks == ticks_expected);
    }

    // UP TO DATE AS OF: 21.06.2025
    SECTION("ticks-26r-8.5s-O/T/N2+F") {
        float mix_pressure = 726.60645f;
        float mix_temp = 112.840805f;
        float thir_temp = 542.761f;
        float oxygen_frac = 0.14539835f;
        float tritium_frac = 0.16864481f;
        float oxide_frac = 0.6859568f;
        float radius_expected = 26.13f;
        size_t ticks_expected = 17;

        tank.mix.temperature = mix_temp;
        tank.mix.adjust_pressure_of(oxygen, mix_pressure * oxygen_frac);
        tank.mix.adjust_pressure_of(tritium, mix_pressure * tritium_frac);
        tank.mix.adjust_pressure_of(nitrous_oxide, mix_pressure * oxide_frac);
        tank.mix.canister_fill_to(frezon, thir_temp, pressure_cap);

        size_t ticks = tank.tick_n(ticks_expected);

        REQUIRE(tank.state == tank.st_exploded);
        REQUIRE(tank.radius_cache == Catch::Approx(radius_expected).epsilon(0.01f));
        REQUIRE(ticks == ticks_expected);
    }
}

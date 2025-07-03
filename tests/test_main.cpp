#include <cmath>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <argparse/args.hpp>

#include "constants.hpp"
#include "gas.hpp"
#include "tank.hpp"
#include "optimiser.hpp"
#include "utility.hpp"

using Catch::Approx;
using namespace asim;

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

TEST_CASE("Vector math operations") {
    SECTION("Vector-vector operations") {
        std::vector<float> a {1.0f, 2.0f, 3.0f};
        std::vector<float> b {4.0f, 5.0f, 6.0f};

        SECTION("Addition") {
            a += b;
            REQUIRE(a == std::vector<float>{5.0f, 7.0f, 9.0f});
            REQUIRE(a - b == std::vector<float>{1.0f, 2.0f, 3.0f});
        }

        SECTION("Subtraction") {
            a -= b;
            REQUIRE(a == std::vector<float>{-3.0f, -3.0f, -3.0f});
            REQUIRE(b - a == std::vector<float>{7.0f, 8.0f, 9.0f});
        }
    }

    SECTION("Vector-scalar operations") {
        std::vector<float> v {2.0f, 4.0f, 6.0f};

        SECTION("Multiplication") {
            REQUIRE(v * 2.0f == std::vector<float>{4.0f, 8.0f, 12.0f});
            REQUIRE(3.0f * v == std::vector<float>{6.0f, 12.0f, 18.0f});
        }
    }

    SECTION("Vector operations") {
        std::vector<float> vec {3.0f, 4.0f, 0.0f};

        SECTION("Normalization") {
            normalize(vec);
            REQUIRE(length(vec) == Approx(1.0f));
            REQUIRE(vec == std::vector<float>{0.6f, 0.8f, 0.0f});
        }

        SECTION("Lerp") {
            std::vector<float> target {5.0f, 6.0f, 7.0f};
            std::vector<float> result = lerp(vec, target, 0.5f);
            REQUIRE(result == std::vector<float>{4.0f, 5.0f, 3.5f});
        }

        SECTION("Length and dot product") {
            REQUIRE(length(vec) == Approx(5.0f));
            REQUIRE(dot(vec, vec) == Approx(25.0f));

            std::vector<float> ortho {4.0f, -3.0f, 0.0f};
            REQUIRE(dot(vec, ortho) == Approx(0.0f).margin(0.001f));
        }
    }

    SECTION("Orthogonal noise") {
        std::vector<float> vec {3.0f, 4.0f, 0.0f};

        std::vector<float> noise = orthogonal_noise(vec, 1.f);
        REQUIRE(length(noise) == Approx(1.f).epsilon(0.001f));
        REQUIRE(dot(vec, noise) == Approx(0.0f).margin(0.001f));
    }
}

TEST_CASE("Gas system performance benchmarks") {
    gas_mixture bench_mix(tank_volume);

    SECTION("total_gas() calculation") {
        bench_mix.canister_fill_to({ {oxygen, 0.2f}, {nitrogen, 0.5f}, {plasma, 0.3f} }, 500.f, 3000.f);

        BENCHMARK("Mols calculation") {
            return bench_mix.total_gas();
        };
    }

    SECTION("pressure() calculation") {
        bench_mix.canister_fill_to({ {oxygen, 0.2f}, {nitrogen, 0.5f}, {plasma, 0.3f} }, 500.f, 3000.f);

        BENCHMARK("Pressure calculation") {
            return bench_mix.pressure();
        };
    }

    SECTION("heat_capacity() calculation") {
        bench_mix.canister_fill_to({ {tritium, 0.4f}, {carbon_dioxide, 0.6f} }, 800.f, 1500.f);

        BENCHMARK("Heat capacity calculation") {
            return bench_mix.heat_capacity();
        };
    }

    SECTION("reaction_tick processing") {
        bench_mix.canister_fill_to({ {oxygen, 10.f}, {plasma, 5.f} }, 2000.f, 1.f);

        BENCHMARK("Process reactions") {
            return bench_mix.reaction_tick();
        };
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

    // UP TO DATE AS OF: 03.07.2025
    SECTION("ticks-12.3r-22.5s-PT+O") {
        float mix_pressure = 684.853f;
        float mix_temp = 382.42734f;
        float plasma_frac = 0.52208485f;
        float tritium_frac = 1.f - plasma_frac;
        float radius_expected = 12.275f;
        size_t ticks_expected = 45;

        std::vector<std::pair<gas_ref, float>> mix = {{plasma, plasma_frac}, {tritium, tritium_frac}};
        tank.mix.canister_fill_to(mix, mix_temp, mix_pressure);
        tank.mix.canister_fill_to(oxygen, T20C, pressure_cap);

        size_t ticks = tank.tick_n(ticks_expected * 2);

        REQUIRE(tank.state == tank.st_exploded);
        REQUIRE(tank.calc_radius() == Catch::Approx(radius_expected).epsilon(0.01f));
        REQUIRE(ticks == ticks_expected);
    }

    // UP TO DATE AS OF: 03.07.2025
    SECTION("ticks-24.6r-8.5s-O/T/N2+F") {
        float mix_pressure = 726.60645f;
        float mix_temp = 112.840805f;
        float thir_temp = 542.761f;
        float oxygen_frac = 0.14539835f;
        float tritium_frac = 0.16864481f;
        float oxide_frac = 0.6859568f;
        float radius_expected = 24.635f;
        size_t ticks_expected = 17;

        std::vector<std::pair<gas_ref, float>> mix = {{oxygen, oxygen_frac}, {tritium, tritium_frac}, {nitrous_oxide, oxide_frac}};
        tank.mix.canister_fill_to(mix, mix_temp, mix_pressure);
        tank.mix.canister_fill_to(frezon, thir_temp, pressure_cap);

        size_t ticks = tank.tick_n(ticks_expected * 2);

        REQUIRE(tank.state == tank.st_exploded);
        REQUIRE(tank.calc_radius() == Catch::Approx(radius_expected).epsilon(0.01f));
        REQUIRE(ticks == ticks_expected);
    }

    // UP TO DATE AS OF: 03.07.2025
    SECTION("ticks-16r-1921s-N2/T+O/F") {
        float mix_pressure = 476.4f;
        float mix_temp = 159.82f;
        float thir_temp = 528.35f;
        float release_p = 788.9f;
        float oxide_frac = 0.4931195f;
        float tritium_frac = 0.50688046f;
        float oxygen_frac = 0.028119187f;
        float frezon_frac = 0.9718808f;
        float radius_expected = 16.037f;
        size_t ticks_expected = 3843;

        std::vector<std::pair<gas_ref, float>> mix = {{nitrous_oxide, oxide_frac}, {tritium, tritium_frac}};
        tank.mix.canister_fill_to(mix, mix_temp, mix_pressure);
        std::vector<std::pair<gas_ref, float>> primer = {{oxygen, oxygen_frac}, {frezon, frezon_frac}};
        tank.mix.canister_fill_to(primer, thir_temp, release_p);

        size_t ticks = tank.tick_n(ticks_expected * 2);

        REQUIRE(tank.state == tank.st_exploded);
        REQUIRE(tank.calc_radius() == Catch::Approx(radius_expected).epsilon(0.01f));
        REQUIRE(ticks == ticks_expected);
    }
}

// wrapper for bomb_data for use by the optimiser
struct float_wrap {
    float data = 0.f;
    bool valid_v = true;

    float_wrap(): valid_v(false) {}
    float_wrap(float f): data(f) {}

    float rating() const {
        return data;
    }
    std::string rating_str() const {
        return std::format("{}", data);
    }
    bool valid() const {
        return valid_v;
    }
    bool operator>(const float_wrap& rhs) const {
        return data > rhs.data;
    }
    bool operator>=(const float_wrap& rhs) const {
        return data >= rhs.data;
    }
    bool operator==(const float_wrap& rhs) const {
        return data == rhs.data;
    }
};

float_wrap opt_sine(const std::vector<float>& in_args, const std::tuple<>&) {
    return {std::sin(in_args[0])};
}

float_wrap opt_fun(const std::vector<float>& in_args, const std::tuple<>&) {
    float x = in_args[0];
    float y = in_args[1];
    float val = std::sin(x*2.0f) * std::cos(y*1.5f) +
                0.5f * std::sin(x*5.0f) * std::cos(y*3.0f) +
                0.2f * std::sin(x*10.0f) * std::cos(y*6.0f);
    return {val};
}

TEST_CASE("Optimiser validation") {
    SECTION("Coordinate descent") {
        SECTION("Sine wave optimisation") {
            optimiser<std::tuple<>, float_wrap>
            optim(opt_sine,
                {-M_PI * 0.5f},
                {M_PI * 0.5f},
                true,
                std::make_tuple(),
                as_seconds(0.001f),
                5,
                0.5f);
            optim.poll_spacing = as_seconds(0.01f);
            optim.fuzzn = 1000;

            SECTION("Minimisation") {
                optim.maximise = false;

                optim.find_best();
                std::vector<float> best_args = optim.best_arg;
                const float_wrap& best_res = optim.best_result;

                REQUIRE(best_res.valid());
                REQUIRE(best_args[0] == Approx(-M_PI * 0.5f).epsilon(0.01f));
                REQUIRE(best_res.data == Approx(-1.f).epsilon(0.01f));
            }

            SECTION("Maximisation") {
                optim.find_best();
                std::vector<float> best_args = optim.best_arg;
                const float_wrap& best_res = optim.best_result;

                REQUIRE(best_res.valid());
                REQUIRE(best_args[0] == Approx(M_PI * 0.5f).epsilon(0.01f));
                REQUIRE(best_res.data == Approx(1.f).epsilon(0.01f));
            }
        }

        SECTION("2-variable optimisation") {
            optimiser<std::tuple<>, float_wrap>
            c_optim(opt_fun,
                {0.f, -0.5f},
                {1.f, 1.5f},
                true,
                std::make_tuple(),
                as_seconds(0.05f),
                5,
                0.5f);
            c_optim.poll_spacing = as_seconds(0.05f);
            c_optim.fuzzn = 10000;

            SECTION("Maximisation") {
                c_optim.find_best();
                std::vector<float> best_args = c_optim.best_arg;
                const float_wrap& best_res = c_optim.best_result;

                REQUIRE(best_res.valid());
                REQUIRE(best_args[0] == Approx(0.292f).epsilon(0.01f));
                REQUIRE(best_args[1] == Approx(0.f).margin(0.01f));
                REQUIRE(best_res.data == Approx(1.092f).epsilon(0.01f));
            }

            SECTION("Minimisation") {
                c_optim.maximise = false;
                c_optim.find_best();
                std::vector<float> best_args = c_optim.best_arg;
                const float_wrap& best_res = c_optim.best_result;

                REQUIRE(best_res.valid());
                REQUIRE(best_args[0] == Approx(0.768f).epsilon(0.01f));
                REQUIRE(best_args[1] == Approx(1.5f).epsilon(0.01f));
                REQUIRE(best_res.data == Approx(-0.74f).epsilon(0.01f));
            }
        }
    }
}

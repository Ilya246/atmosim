#pragma once

#include <tomlplusplus/toml.hpp>

namespace asim {

inline static toml::table config = []() {
    char* path = std::getenv("ATMOSIM_CONFIG");
    if (path != nullptr)
        try {
            return toml::parse_file(path);
        } catch (...) { }
    return toml::table();
}();

inline const float // goobstation (non-reforged) defaults, up to date as of 14.02.2026
// [Atmosim]
default_tol = config["Atmosim"]["DefaultTolerance"].value_or(0.95f),

// [Cvars]
heat_scale = config["Cvars"]["HeatScale"].value_or(1.0 / 8.f), // inverted

// [Atmospherics]
R = config["Atmospherics"]["R"].value_or(8.314462618f),
one_atmosphere = config["Atmospherics"]["OneAtmosphere"].value_or(101.325f),
TCMB = config["Atmospherics"]["TCMB"].value_or(2.7f),
T0C = config["Atmospherics"]["T0C"].value_or(273.15f),
T20C = config["Atmospherics"]["T20C"].value_or(293.15f),
minimum_heat_capacity = config["Atmospherics"]["MinimumHeatCapacity"].value_or(0.0003f),

// [Plasma]
fire_plasma_energy_released = config["Plasma"]["FireEnergyReleased"].value_or(160000.f) * heat_scale,
super_saturation_threshold = config["Plasma"]["SuperSaturationThreshold"].value_or(96.f),
super_saturation_ends = config["Plasma"]["SuperSaturationEnds"].value_or(super_saturation_threshold / 3.f),
oxygen_burn_rate_base = config["Plasma"]["OxygenBurnRateBase"].value_or(1.4f),
plasma_minimum_burn_temperature = config["Plasma"]["MinimumBurnTemperature"].value_or(100.f + T0C),
plasma_upper_temperature = config["Plasma"]["UpperTemperature"].value_or(1370.f + T0C),
plasma_oxygen_fullburn = config["Plasma"]["OxygenFullburn"].value_or(10.f),
plasma_burn_rate_delta = config["Plasma"]["BurnRateDelta"].value_or(9.f),

// [Tritium]
fire_hydrogen_energy_released = config["Tritium"]["FireEnergyReleased"].value_or(284000.f) * heat_scale,
minimum_tritium_oxyburn_energy = config["Tritium"]["MinimumOxyburnEnergy"].value_or(143000.f) * heat_scale,
tritium_burn_oxy_factor = config["Tritium"]["BurnOxyFactor"].value_or(100.f),
tritium_burn_trit_factor = config["Tritium"]["BurnTritFactor"].value_or(10.f),
tritium_burn_fuel_ratio = config["Tritium"]["BurnFuelRatio"].value_or(0.f),

// [Frezon]
frezon_cool_lower_temperature = config["Frezon"]["CoolLowerTemperature"].value_or(23.15f),
frezon_cool_mid_temperature = config["Frezon"]["CoolMidTemperature"].value_or(373.15f),
frezon_cool_maximum_energy_modifier = config["Frezon"]["CoolMaximumEnergyModifier"].value_or(10.f),
frezon_nitrogen_cool_ratio = config["Frezon"]["NitrogenCoolRatio"].value_or(5.f),
frezon_cool_energy_released = config["Frezon"]["CoolEnergyReleased"].value_or(-600000.f) * heat_scale,
frezon_cool_rate_modifier = config["Frezon"]["CoolRateModifier"].value_or(20.f),
frezon_production_temp = config["Frezon"]["ProductionTemp"].value_or(73.15f),
frezon_production_max_efficiency_temperature = config["Frezon"]["ProductionMaxEfficiencyTemperature"].value_or(73.15f),
frezon_production_nitrogen_ratio = config["Frezon"]["ProductionNitrogenRatio"].value_or(10.f),
frezon_production_trit_ratio = config["Frezon"]["ProductionTritRatio"].value_or(50.f),
frezon_production_conversion_rate = config["Frezon"]["ProductionConversionRate"].value_or(50.f),

// [N20]
N2Odecomposition_rate = config["N20"]["DecompositionRate"].value_or(1.f / 2.f), // inverted

// [Nitrium]
nitrium_decomposition_energy = config["Nitrium"]["DecompositionEnergy"].value_or(30000.f),

// [Reactions]
reaction_min_gas = config["Reactions"]["ReactionMinGas"].value_or(0.01f),
plasma_fire_temp = config["Reactions"]["PlasmaFireTemp"].value_or(373.149f),
trit_fire_temp = config["Reactions"]["TritiumFireTemp"].value_or(373.149f),
frezon_cool_temp = config["Reactions"]["FrezonCoolTemp"].value_or(23.15f),
n2o_decomp_temp = config["Reactions"]["N2ODecomposionTemp"].value_or(850.f),
nitrium_decomp_temp = config["Reactions"]["NitriumDecompositionTemp"].value_or(T0C + 70.f),

// [Canister]
pressure_cap = config["Canister"]["TransferPressureCap"].value_or(1013.25f),
required_transfer_volume = config["Canister"]["RequiredTransferVolume"].value_or(1500.f + 200.f * 2), // canister + two pipes volume

// [Tank]
tank_volume = config["Tank"]["Volume"].value_or(5.f),
tank_leak_pressure = config["Tank"]["LeakPressure"].value_or(30.f * one_atmosphere),
tank_rupture_pressure = config["Tank"]["RupturePressure"].value_or(40.f * one_atmosphere),
tank_fragment_pressure = config["Tank"]["FragmentPressure"].value_or(50.f * one_atmosphere),
tank_fragment_scale = config["Tank"]["FragmentScale"].value_or(2.25f * one_atmosphere),

// [Misc]
tickrate = config["Misc"]["Tickrate"].value_or(0.5f);

inline const size_t round_temp_dig = 2, round_pressure_dig = 1;

}

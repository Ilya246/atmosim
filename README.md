![Latest version](https://img.shields.io/github/v/tag/Ilya246/atmosim?label=Latest%20version)

# Usage instructions

## Getting a ready release

1. Go to the [releases tab](https://github.com/Ilya246/atmosim/releases)
2. Get the latest release depending on your system

## Compiling it yourself (will run faster)

1. Ensure you have git, make and a C++ compiler installed
2. Clone the repository with submodules:
```bash
git clone --recursive https://github.com/ilya246/atmosim
cd atmosim
```
3. Build using the Makefile:
```bash
make -j
```

## Using AUR (on Arch Linux)
![AUR version](https://img.shields.io/aur/version/atmosim?label=AUR%20version)
```bash
yay -S atmosim
```
This also installs configs into /etc/atmosim/

## Running

1. Open a console (on Windows press win+r then type in `cmd`), then `cd .../path/to/atmosim`, path/to/atmosim being its location on your disk
2. Run `atmosim -h` or `atmosim.exe -h`
3. Refer to the help readout for further usage instructions

### zsh
Zsh (default on MacOS) treats [] as pattern matching. And since atmosim takes many arguments in [], you would need to overwrite this behaviour by wrapping [] with "":
`-mg="[plasma,tritium]"`

## Configuring constants

(nearly) all the constants can be configured in a toml file, in case if they are different on a fork.
By default, atmosim reads the configuration.toml file from the current working directory.
You can overwrite this with ATMOSIM_CONFIG enviroment variable. In cmd on Windows:
```
set ATMOSIM_CONFIG=configs/monolith.toml
out/atmosim.exe
```
Bash:
`ATMOSIM_CONFIG=$HOME/.config/atmosim/my_fork.toml ./atmosim`.
Here are the options:
```
[Atmosim]
DefaultTolerance

[Cvars]
HeatScale

[Atmospherics]
R
OneAtmosphere
TCMB
T0C
T20C
MinimumHeatCapacity

[Plasma]
FireEnergyReleased
SuperSaturationThreshold
SuperSaturationEnds
OxygenBurnRateBase
MinimumBurnTemperature
UpperTemperature
OxygenFullburn
BurnRateDelta

[Tritium]
FireEnergyReleased
MinimumOxyburnEnergy
BurnOxyFactor
BurnTritFactor
BurnFuelRatio # 0 by default, which uses pre https://github.com/space-wizards/space-station-14/pull/41870 logic

[Frezon]
CoolLowerTemperature
CoolMidTemperature
CoolMaximumEnergyModifier
NitrogenCoolRatio
CoolEnergyReleased
CoolRateModifier
ProductionTemp
ProductionMaxEfficiencyTemperature
ProductionNitrogenRatio
ProductionTritRatio
ProductionConversionRate

[N20]
DecompositionRate

[Nitrium]
DecompositionEnergy

[Reactions]
ReactionMinGas
PlasmaFireTemp
TritiumFireTemp
FrezonCoolTemp
N2ODecomposionTemp
NitriumDecompositionTemp

[Canister]
TransferPressureCap
RequiredTransferVolume

[Tank]
Volume
LeakPressure
RupturePressure
FragmentPressure
FragmentScale

[Misc]
Tickrate
```

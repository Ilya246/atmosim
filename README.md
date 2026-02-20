![Latest version](https://img.shields.io/github/v/tag/Ilya246/atmosim?label=Latest%20version)

# Usage instructions

## Getting a ready release

1. Go to the [releases tab](https://github.com/Ilya246/atmosim/releases)
2. Get the latest release depending on your system
3. Unpack it

## Compiling it yourself

1. Ensure you have git, make, cmake and a POSIX-compliant C++ compiler (g++, clang or even zig c++) installed. On Windows, use MinGW g++. MSVC is explicitly not supported.
2. Clone the repository with submodules:
```bash
git clone --recursive https://github.com/ilya246/atmosim
cd atmosim
```
3. Build using the Makefile:
```bash
make -j release
```

Given you have MinGW, you can also cross-compile from Linux to Windows with
```
```
make -j win
```
```
Other kinds of cross-compiling are not supported, but feel free to implement and PR them.

## Using AUR (on Arch Linux)
![AUR version](https://img.shields.io/aur/version/atmosim?label=AUR%20version)
```bash
yay -S atmosim # or whatever AUR helper you're using
```
This also installs configs into /etc/atmosim/

## Running

This assumes you use Windows, if you don't you probably know this anyway.
1. Open a terminal (Win+R `cmd`) and cd into C:\the\folder\with\the\program.
2. Run `atmosim.exe -h`
3. Refer to the help readout for further usage instructions

### zsh
Zsh treats [] as pattern matching. And since atmosim takes many arguments in [], you would need to overwrite this behaviour by wrapping [] with "":
`-mg="[plasma,tritium]"`

## Configuring constants

(nearly) all the constants can be configured in a toml file, in case if they are different on a fork.
Set the path to your ATMOSIM_CONFIG file as the enviroment variable.
Windows cmd:
```
set ATMOSIM_CONFIG=configs\monolith.toml
atmosim.exe
```
Bash:
`ATMOSIM_CONFIG=$HOME/.config/atmosim/my_fork.toml ./atmosim`
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

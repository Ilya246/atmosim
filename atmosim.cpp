#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <set>
#include <unordered_map>
#include <vector>

using namespace std;

enum ValueType {IntVal, FloatVal, BoolVal, NoneVal};

struct DynVal {
	ValueType type;
	void* valuePtr;

	bool valid() {
		return type != NoneVal
		&&     valuePtr != nullptr;
	}
};

template <typename T>
T* getDynPtr(DynVal val) {
	return (T*)val.valuePtr;
}
template <typename T>
T& getDyn(DynVal val) {
	return *getDynPtr<T>(val);
}

// generic system for specifying what you don't want atmosim to give you
struct BaseRestriction {
	virtual bool OK() = 0;
};

template <typename T>
struct NumRestriction : BaseRestriction {
	T* valuePtr;
	T minValue;
	T maxValue;

	NumRestriction(T* ptr, T min, T max): valuePtr(ptr), minValue(min), maxValue(max) {}

	bool OK() override {
		return *valuePtr >= minValue
		&&     *valuePtr <= maxValue;
	}
};

struct BoolRestriction : BaseRestriction {
	bool* valuePtr;
	bool targetValue;

	BoolRestriction(bool* ptr, bool target): valuePtr(ptr), targetValue(target) {}

	bool OK() override {
		return *valuePtr == targetValue;
	}
};

float heatScale = 1.0;

const int gas_count = 6;
float gasAmounts[gas_count]{};
float gasHeatCaps[gas_count]{20.f * heatScale, 200.f * heatScale, 10.f * heatScale, 40.f * heatScale, 30.f * heatScale, 600.f * heatScale};
string gasNames[gas_count]{  "oxygen",         "plasma",          "tritium",        "waterVapour",    "carbonDioxide",  "frezon"         };

struct GasType {
	int gas;

	float& amount() {
		return gasAmounts[gas];
	}

	float& heatCap() {
		return gasHeatCaps[gas];
	}

	string name() {
		return gasNames[gas];
	}
};

GasType oxygen{0};
GasType plasma{1};
GasType tritium{2};
GasType waterVapour{3};
GasType carbonDioxide{4};
GasType frezon{5};
GasType invalidGas{-1};

GasType gases[]{oxygen, plasma, tritium, waterVapour, carbonDioxide, frezon};

unordered_map<string, GasType> gasMap{
	{"oxygen",        oxygen       },
	{"plasma",        plasma       },
	{"tritium",       tritium      },
	{"waterVapour",   waterVapour  },
	{"carbonDioxide", carbonDioxide},
	{"frezon",        frezon       }};

string listGases() {
	string out;
	for (GasType g : gases) {
		out += g.name() + ", ";
	}
	out += gases[gas_count - 1].name();
	return out;
}

enum TankState {
	intact = 0,
	ruptured = 1,
	exploded = 2
};

float temperature = 293.15, volume = 5.0, pressureCap = 1013.25, pipePressureCap = 4500.0, requiredTransferVolume = 1400.0,
radius = 0.0,
leakedHeat = 0.0;
TankState tankState = intact;
int integrity = 3, leakCount = 0, tick = 0,
tickCap = 30, pipeTickCap = 1000,
logLevel = 1;
bool stepTargetTemp = false,
checkStatus = true,
optimiseInt = false, optimiseMaximise = true, optimiseBefore = false;
float fireTemp = 373.15, minimumHeatCapacity = 0.0003, oneAtmosphere = 101.325, R = 8.314462618,
tankLeakPressure = 30.0 * oneAtmosphere, tankRupturePressure = 40.0 * oneAtmosphere, tankFragmentPressure = 50.0 * oneAtmosphere, tankFragmentScale = 2.0 * oneAtmosphere,
fireHydrogenEnergyReleased = 284000.0 * heatScale, minimumTritiumOxyburnEnergy = 143000.0, tritiumBurnOxyFactor = 100.0, tritiumBurnTritFactor = 10.0,
firePlasmaEnergyReleased = 160000.0 * heatScale, superSaturationThreshold = 96.0, superSaturationEnds = superSaturationThreshold / 3.0, oxygenBurnRateBase = 1.4, plasmaUpperTemperature = 1643.15, plasmaOxygenFullburn = 10.0, plasmaBurnRateDelta = 9.0,
tickrate = 0.5,
overTemp = 0.1, temperatureStep = 1.005, temperatureStepMin = 0.5, ratioStep = 1.01, ratioFrom = 10.0, ratioTo = 10.0;
string selectedGases[]{"", "", ""};
string rotator = "|/-\\";
int rotatorChars = 4;
int rotatorIndex = rotatorChars - 1;

DynVal optimiseVal = {FloatVal, &radius};
vector<BaseRestriction*> preRestrictions;
vector<BaseRestriction*> postRestrictions;

bool restrictionsMet(vector<BaseRestriction*> restrictions) {
	for (BaseRestriction* r : restrictions) {
		if (!r->OK()) {
			return false;
		}
	}
	return true;
}

char getRotator() {
	rotatorIndex = (rotatorIndex + 1) % rotatorChars;
	return rotator[rotatorIndex];
}

unordered_map<string, DynVal> simParams{
	{"radius",      {FloatVal, &radius     }},
	{"temperature", {FloatVal, &temperature}},
	{"leakedHeat",  {FloatVal, &leakedHeat }},
	{"ticks",       {IntVal,   &tick       }},
	{"tankState",   {IntVal,   &tankState  }}};

// ran at the start of main()
void setupParams() {
	for (GasType g : gases) {
		simParams["gases." + g.name()] = {FloatVal, &g.amount()};
	}
}

string listParams() {
	string out;
	for (const auto& [key, value] : simParams) {
		out += key + ", ";
	}
	out.resize(out.size() - 2);
	return out;
}

DynVal getParam(string name) {
	if (simParams.contains(name)) {
		return simParams[name];
	}
	return {NoneVal, nullptr};
}

bool evalOpt(string opt, bool default_opt) {
	return opt == "y" || opt == "Y"
	||    (opt != "n" && opt != "N" && default_opt);
}

bool evalOpt(string opt) {
	return evalOpt(opt, true);
}

void reset() {
	for (GasType g : gases) {
		g.amount() = 0.0;
	}
	temperature = 293.15;
	tankState = intact;
	integrity = 3;
	tick = 0;
	leakCount = 0;
	radius = 0.0;
	leakedHeat = 0.0;
}

bool isGas(string gas) {
	return gasMap.contains(gas);
}

// string-to-gas
GasType sToG(string gas) {
	if (!isGas(gas)) {
		return invalidGas;
	}
	return gasMap[gas];
}

float getHeatCapacity() {
	float sum = 0.0;
	for (GasType g : gases) {
		sum += g.amount() * g.heatCap();
	}
	return sum;
}
float getGasMols() {
	float sum = 0.0;
	for (GasType g : gases) {
		sum += g.amount();
	}
	return sum;
}
float pressureTempToMols(float pressure, float temp) {
	return pressure * volume / temp / R;
}
float molsTempToPressure(float mols, float temp) {
	return mols * R * temp / volume;
}
float gasesTempsToTemp(GasType gas1, float temp1, GasType gas2, float temp2) {
	return (gas1.amount() * temp1 * gas1.heatCap() + gas2.amount() * temp2 * gas2.heatCap()) / (gas1.amount() * gas1.heatCap() + gas2.amount() * gas2.heatCap());
}
float mixGasTempsToTemp(float gasc1, float gashc1, float temp1, GasType gas2, float temp2) {
	return (gasc1 * temp1 * gashc1 + gas2.amount() * temp2 * gas2.heatCap()) / (gasc1 * gashc1 + gas2.amount() * gas2.heatCap());
}
float getPressure() {
	return getGasMols() * R * temperature / volume;
}
float getCurRange() {
	return sqrt((getPressure() - tankFragmentPressure) / tankFragmentScale);
}

void checkDoPlasmaFire() {
	if (temperature < fireTemp) return;
	if (std::min(oxygen.amount(), plasma.amount()) < 0.01) return;
	float energyReleased = 0.0;
	float oldHeatCapacity = getHeatCapacity();
	float temperatureScale = 0.0;
	if (temperature > plasmaUpperTemperature) {
		temperatureScale = 1.0;
	} else {
		temperatureScale = (temperature - fireTemp) / (plasmaUpperTemperature - fireTemp);
	}
	if (temperatureScale > 0) {
		float oxygenBurnRate = oxygenBurnRateBase - temperatureScale;
		float plasmaBurnRate = temperatureScale * (oxygen.amount() > plasma.amount() * plasmaOxygenFullburn ? plasma.amount() / plasmaBurnRateDelta : oxygen.amount() / plasmaOxygenFullburn / plasmaBurnRateDelta);
		if (plasmaBurnRate > minimumHeatCapacity) {
			plasmaBurnRate = std::min(plasmaBurnRate, std::min(plasma.amount(), oxygen.amount() / oxygenBurnRate));
			float supersaturation = std::min(1.0f, std::max((oxygen.amount() / plasma.amount() - superSaturationEnds) / (superSaturationThreshold - superSaturationEnds), 0.0f));
			plasma.amount() -= plasmaBurnRate;
			oxygen.amount() -= plasmaBurnRate * oxygenBurnRate;
			tritium.amount() += plasmaBurnRate * supersaturation;
			carbonDioxide.amount() += plasmaBurnRate * (1.0f - supersaturation);

			energyReleased += firePlasmaEnergyReleased * plasmaBurnRate;
		}
	}
	if (energyReleased > 0.0) {
		float newHeatCapacity = getHeatCapacity();
		if (newHeatCapacity > minimumHeatCapacity) {
			temperature = (temperature * oldHeatCapacity + energyReleased) / newHeatCapacity;
		}
	}
}
void checkDoTritFire() {
	if (temperature < fireTemp) return;
	if (std::min(tritium.amount(), oxygen.amount()) < 0.01) return;
	float energyReleased = 0.0;
	float oldHeatCapacity = getHeatCapacity();
	float burnedFuel = 0.0;
	if (oxygen.amount() < tritium.amount() || minimumTritiumOxyburnEnergy > temperature * oldHeatCapacity) {
		burnedFuel = std::min(tritium.amount(), oxygen.amount() / tritiumBurnOxyFactor);
		tritium.amount() -= burnedFuel;
	} else {
		burnedFuel = tritium.amount();
		tritium.amount() *= 1.0 - 1.0 / tritiumBurnTritFactor;
		oxygen.amount() -= tritium.amount();
		energyReleased += fireHydrogenEnergyReleased * burnedFuel * (tritiumBurnTritFactor - 1.0);
	}
	if (burnedFuel > 0.0) {
		energyReleased += fireHydrogenEnergyReleased * burnedFuel;
		waterVapour.amount() += burnedFuel;
	}
	if (energyReleased > 0.0) {
		float newHeatCapacity = getHeatCapacity();
		if (newHeatCapacity > minimumHeatCapacity) {
			temperature = (temperature * oldHeatCapacity + energyReleased) / newHeatCapacity;
		}
	}
}

void react() {
	checkDoTritFire();
	checkDoPlasmaFire();
}
void tankCheckStatus() {
	float pressure = getPressure();
	if (pressure > tankFragmentPressure) {
		for (int i = 0; i < 3; ++i) {
			react();
		}
		tankState = exploded;
		radius = getCurRange();
		for (GasType g : gases) {
			leakedHeat += g.amount() * g.heatCap() * temperature;
		}
		return;
	}
	if (pressure > tankRupturePressure) {
		if (integrity <= 0) {
			tankState = ruptured;
			radius = 0.0;
			for (GasType g : gases) {
				leakedHeat += g.amount() * g.heatCap() * temperature;
			}
			return;
		}
		integrity--;
		return;
	}
	if (pressure > tankLeakPressure) {
		if (integrity <= 0) {
			for (GasType g : gases) {
				leakedHeat += g.amount() * g.heatCap() * temperature * 0.25;
				g.amount() *= 0.75;
			}
			leakCount++;
		} else {
			integrity--;
		}
		return;
	}
	if (integrity < 3) {
		integrity++;
	}
}

void status() {
	cout << "TICK: " << tick << " || Status: pressure " << getPressure() << "kPa \\ integrity " << integrity << " \\ temperature " << temperature << "K\nContents: " << oxygen.amount() << " o2 \\ " << plasma.amount() << " p \\ " << tritium.amount() << " t \\ " << waterVapour.amount() << " w \\ " << carbonDioxide.amount() << " co2" << endl;
	if (tankState == exploded) {
		cout << "EXPLOSION: range " << getCurRange() << endl;
	}
}

void loop(int n) {
	while (tick < n) {
		react();
		++tick;
	}
}
void loop() {
	if (!checkStatus) {
		loop(tickCap);
		return;
	}
	while (tick < tickCap && tankState == intact) {
		react();
		tankCheckStatus();
		++tick;
	}
}
void loopPrint() {
    while (tick < tickCap && tankState == intact) {
		react();
        tankCheckStatus();
		++tick;
        status();
	}
}


void fullInputSetup() {
	float tpressure, ttemp, sumheat;
	cout << "Oxygen kPa + K: " << endl;
	cin >> tpressure >> ttemp;
	oxygen.amount() = pressureTempToMols(tpressure, ttemp);
	sumheat += oxygen.amount() * oxygen.heatCap() * ttemp;
	cout << "Plasma kPa + K: " << endl;
	cin >> tpressure >> ttemp;
	plasma.amount() = pressureTempToMols(tpressure, ttemp);
	sumheat += plasma.amount() * plasma.heatCap() * ttemp;
	cout << "Trit kPa + K: " << endl;
	cin >> tpressure >> ttemp;
	tritium.amount() = pressureTempToMols(tpressure, ttemp);
	sumheat += tritium.amount() * tritium.heatCap() * ttemp;
	cout << "frezon kPa + K: " << endl;
	cin >> tpressure >> ttemp;
	frezon.amount() = pressureTempToMols(tpressure, ttemp);
	sumheat += frezon.amount() * frezon.heatCap() * ttemp;
	temperature = sumheat / getHeatCapacity();
}
float mixInputSetup(GasType gas1, GasType gas2, GasType into, float fuelTemp, float intoTemp, float targetTemp, float secondPerFirst) {
	float specheat = (gas1.heatCap() + gas2.heatCap() * secondPerFirst) / (1.0 + secondPerFirst);
	float fuelPressure = (targetTemp / intoTemp - 1.0) * pressureCap / (specheat / into.heatCap() - 1.0 + targetTemp * (1.0 / intoTemp - specheat / into.heatCap() / fuelTemp));
	float fuel = pressureTempToMols(fuelPressure, fuelTemp);
	gas1.amount() = fuel / (1.0 + secondPerFirst);
	gas2.amount() = fuel - gas1.amount();
	into.amount() = pressureTempToMols(pressureCap - fuelPressure, intoTemp);
	temperature = mixGasTempsToTemp(fuel, specheat, fuelTemp, into, intoTemp);
	return fuelPressure;
}
void knownInputSetup(GasType gas1, GasType gas2, GasType into, float fuelTemp, float intoTemp, float fuelPressure, float secondPerFirst) {
	float specheat = (gas1.heatCap() + gas2.heatCap() * secondPerFirst) / (1.0 + secondPerFirst);
	float fuel = pressureTempToMols(fuelPressure, fuelTemp);
	gas1.amount() = fuel / (1.0 + secondPerFirst);
	gas2.amount() = fuel - gas1.amount();
	into.amount() = pressureTempToMols(pressureCap - fuelPressure, intoTemp);
	temperature = mixGasTempsToTemp(fuel, specheat, fuelTemp, into, intoTemp);
}
float unimixInputSetup(GasType gas1, GasType gas2, float temp1, float temp2, float targetTemp) {
	float fuelPressure = (targetTemp / temp2 - 1.0) * pressureCap / (gas1.heatCap() / gas2.heatCap() - 1.0 + targetTemp * (1.0 / temp2 - gas1.heatCap() / gas2.heatCap() / temp1));
	gas1.amount() = pressureTempToMols(fuelPressure, temp1);
	gas2.amount() = pressureTempToMols(pressureCap - fuelPressure, temp2);
	temperature = mixGasTempsToTemp(gas1.amount(), gas1.heatCap(), temp1, gas2, temp2);
	return fuelPressure;
}
void unimixToInputSetup(GasType gas1, GasType gas2, float temp, float secondPerFirst) {
	temperature = temp;
	float total = pressureTempToMols(pressureCap, temperature);
	gas1.amount() = total / (1.0 + secondPerFirst);
	gas2.amount() = total - gas1.amount();
}

struct BombData {
	GasType gas1, gas2, gas3;
	float ratio, fuelTemp, fuelPressure, thirTemp, mixPressure, mixTemp;
	TankState state = intact;
	float radius, finTemp, finPressure, finHeatLeak, optstat;
	int ticks;

	BombData(float ratio, float fuelTemp, float fuelPressure, float thirTemp, float mixPressure, float mixTemp, GasType gas1, GasType gas2, GasType gas3):
		ratio(ratio), fuelTemp(fuelTemp), fuelPressure(fuelPressure), thirTemp(thirTemp), mixPressure(mixPressure), mixTemp(mixTemp), gas1(gas1), gas2(gas2), gas3(gas3) {};

	void results(float n_radius, float n_finTemp, float n_finPressure, float n_optstat, int n_ticks, TankState n_state) {
		radius = n_radius;
		finTemp = n_finTemp;
		finPressure = n_finPressure;
		optstat = n_optstat;
		ticks = n_ticks;
		state = n_state;
	}

	string printSimple() {
		float firstFraction = 1.f / (1.f + ratio);
		float secondFraction = ratio * firstFraction;
		return string(
		"TANK: { " ) +
			"mix: [ " +
				to_string(100.f * firstFraction) + "%:" + to_string(100.f * secondFraction) + "% | " +
				"temp " + to_string(fuelTemp) + "K | " +
				"pressure " + to_string(fuelPressure) + "kPa " +
			"]; " +
			"third: [ " +
				"temp " + to_string(thirTemp) + "K " +
			"]; " +
			"end state: [ " +
				"ticks " + to_string(ticks) + "t | " + (
				state == exploded ?
				"radius " + to_string(radius) + "til "
				: state == ruptured ? "ruptured " : "no explosion " ) +
			"] " +
			"optstat: " + to_string(optstat) + " ";
		"}";
	}

	string printExtensive() {
		float firstFraction = 1.f / (1.f + ratio);
		float secondFraction = ratio * firstFraction;
		float volumeRatio = (requiredTransferVolume + volume) / volume;
		float addedRatio = (requiredTransferVolume + volume) / requiredTransferVolume;
		return string(
		"TANK: {\n" ) +
		"	initial state: [ " +
				"temp " + to_string(mixTemp) + "K | " +
				"pressure " + to_string(mixPressure) + "kPa | " +
				to_string(pressureTempToMols(firstFraction * fuelPressure, fuelTemp)) + "mol " + gas1.name() + " | " +
				to_string(pressureTempToMols(secondFraction * fuelPressure, fuelTemp)) + "mol " + gas2.name() + " | " +
				to_string(pressureTempToMols(pressureCap - fuelPressure, thirTemp)) + "mol " + gas3.name() + " " +
			"];\n" +
		"	end state: [ " +
				"ticks " + to_string(ticks) + "t | " +
				"finPressure " + to_string(finPressure) + "kPa | " +
				"finTemp " + to_string(finTemp) + "K | " + (
				state == exploded ?
				"radius " + to_string(radius) + "til "
				: state == ruptured ? "ruptured " : "no explosion " ) +
			"]\n" +
		"	optstat: " + to_string(optstat) + "\n" +
		"}\n" +
		"REQUIREMENTS: {\n" +
		"	mix-canister (fuel): [ " +
				to_string(100.f * firstFraction) + "%:" + to_string(100.f * secondFraction) + "%=" + to_string(1.0/ratio) + " " + gas1.name() + ":" + gas2.name() + " | " +
				"temp " + to_string(fuelTemp) + "K | " +
				"release pressure " + to_string(fuelPressure) + "kPa | " +
				"least-mols: [ " + to_string(pressureTempToMols(firstFraction * fuelPressure, fuelTemp) * volumeRatio) + "mol " + gas1.name() + " | " +
				to_string(pressureTempToMols(secondFraction * fuelPressure, fuelTemp) * volumeRatio) + "mol " + gas2.name() + " " +
			"];\n" +
		"	third-canister (primer): [ " +
				"temp " + to_string(thirTemp) + "K | " +
				"pressure " + to_string((pressureCap * 2.0 - fuelPressure) * addedRatio) + "kPa | " +
				"least-mols: " + to_string(pressureTempToMols(pressureCap * 2.0 - fuelPressure, thirTemp) * volumeRatio) + "mol " + gas3.name() + " " +
			"]\n" +
		"}\n" +
		"REVERSE-REQUIREMENTS: {\n" +
		"	mix-canister (primer): [ " +
				"pressure " + to_string((pressureCap + fuelPressure) * addedRatio) + "kPa | " +
				"least-mols: [ " + to_string(pressureTempToMols(pressureCap + firstFraction * fuelPressure, fuelTemp) * volumeRatio) + "mol " + gas1.name() + " | " +
				to_string(pressureTempToMols(pressureCap + secondFraction * fuelPressure, fuelTemp) * volumeRatio) + "mol " + gas2.name() + " " +
			"];\n" +
		"	third-canister (fuel): [ " +
				"temp " + to_string(thirTemp) + "K | " +
				"release pressure " + to_string((pressureCap - fuelPressure)) + "kPa | " +
				"least-mols: " + to_string(pressureTempToMols(pressureCap - fuelPressure, thirTemp) * volumeRatio) + "mol " + gas3.name() + " " +
			"]\n" +
		"}";
	}
};

float optimiseStat() {
	return optimiseVal.type == FloatVal ? getDyn<float>(optimiseVal) : getDyn<int>(optimiseVal);
}
BombData testTwomix(GasType gas1, GasType gas2, GasType gas3, float mixt1, float mixt2, float thirt1, float thirt2, bool maximise, bool measureBefore) {
	// parameters of the tank with the best result we have so far
	BombData bestBomb(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, gas1, gas2, gas3);
	bestBomb.optstat = maximise ? numeric_limits<float>::min() : numeric_limits<float>::max();

	// same but only best in the current surrounding frame
	BombData bestBombLocal(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, gas1, gas2, gas3);
	bestBombLocal.optstat = maximise ? numeric_limits<float>::min() : numeric_limits<float>::max();

	for (float thirTemp = thirt1; thirTemp <= thirt2; thirTemp = std::max(thirTemp * temperatureStep, thirTemp + temperatureStepMin)) {
		for (float fuelTemp = mixt1; fuelTemp <= mixt2; fuelTemp = std::max(fuelTemp * temperatureStep, fuelTemp + temperatureStepMin)) {
			float targetTemp2 = stepTargetTemp ? std::max(thirTemp, fuelTemp) : fireTemp + overTemp + temperatureStep;
			for (float targetTemp = fireTemp + overTemp; targetTemp < targetTemp2; targetTemp = std::max(targetTemp * temperatureStep, targetTemp + temperatureStepMin)) {
				for (float ratio = 1.0 / ratioFrom; ratio <= ratioTo; ratio *= ratioStep) {
					float fuelPressure, stat;
					reset();
					if (fuelTemp <= fireTemp && thirTemp <= fireTemp) {
						continue;
					}
					if ((targetTemp > fuelTemp) == (targetTemp > thirTemp)) {
						continue;
					}
					fuelPressure = mixInputSetup(gas1, gas2, gas3, fuelTemp, thirTemp, targetTemp, ratio);
					if (fuelPressure > pressureCap || fuelPressure < 0.0) {
						continue;
					}
					if (!restrictionsMet(preRestrictions)) {
						continue;
					}
					if (measureBefore) {
						stat = optimiseStat();
					}
					float mixPressure = getPressure();
					loop();
					if (!measureBefore) {
						stat = optimiseStat();
					}
					bool noDiscard = restrictionsMet(postRestrictions);
					BombData curBomb(ratio, fuelTemp, fuelPressure, thirTemp, mixPressure, targetTemp, gas1, gas2, gas3);
					curBomb.results(radius, temperature, getPressure(), stat, tick, tankState);
					if (noDiscard && (maximise == (stat > bestBomb.optstat))) {
						bestBomb = curBomb;
					}
					if (logLevel >= 5) {
						if (logLevel == 5) {
							cout << getRotator() << " Current: " << curBomb.printSimple() << endl;
						} else {
							cout << "\n" << curBomb.printExtensive() << endl;
						}
					} else if (noDiscard && (maximise == (stat > bestBombLocal.optstat))) {
						bestBombLocal = curBomb;
					}
				}
				if (logLevel == 4) {
					cout << getRotator() << " Current: " << bestBombLocal.printSimple() << endl;
					bestBombLocal.optstat = maximise ? numeric_limits<float>::min() : numeric_limits<float>::max();
				}
			}
			if (logLevel == 3) {
				cout << getRotator() << " Current: " << bestBombLocal.printSimple() << endl;
				bestBombLocal.optstat = maximise ? numeric_limits<float>::min() : numeric_limits<float>::max();
			}
		}
		if (logLevel == 2) {
			cout << getRotator() << " Current: " << bestBombLocal.printSimple() << endl;
			bestBombLocal.optstat = maximise ? numeric_limits<float>::min() : numeric_limits<float>::max();
		} else if (logLevel == 1) {
			cout << getRotator() << " Best: " << bestBomb.printSimple() << endl;
		}
	}
    return bestBomb;
}

void heatCapInputSetup() {
	cout << "Enter heat capacities for " << listGases() << ": ";
	for (GasType g : gases) {
		cin >> g.heatCap();
	};
}

template <typename T>
auto getMinMaxLimit() {
	struct result {T minv; T maxv;};
	T minv, maxv;
	cout << "Enter lower limit: ";
	cin >> minv;
	cout << "Enter upper limit: ";
	cin >> maxv;
	return result{minv, maxv};
}

void showHelp() {
	cout <<
		"options:\n" <<
		"	-h\n" <<
		"		show help and exit\n" <<
		"	-n\n" <<
		"		assume inside pipe: prevent tank-related effects" <<
		"	-H\n" <<
		"		redefine heat capacities\n" <<
		"	-r\n" <<
		"		set gas ratio iteration bounds+step\n" <<
		"	-s\n" <<
		"		provide potentially better results by also iterating the mix-to temperature (WARNING: will take many times longer to calculate)\n" <<
		"	-m\n" <<
		"		different-temperature gas mixer ratio calculator\n" <<
		"	-f\n" <<
		"		try full input: lets you manually input and test a tank's contents\n" <<
		"	--ticks <value>\n" <<
		"		set tick limit: aborts if a bomb takes longer than this to detonate: default " << tickCap << "\n" <<
		"	--tstep <value>\n" <<
		"		set temperature iteration multiplier: default " << temperatureStep << "\n" <<
        "	--tstepm <value>\n" <<
		"		set minimum temperature iteration step: default " << temperatureStepMin << "\n" <<
		"	--volume <value>\n" <<
		"		set tank volume: default " << volume << "\n" <<
		"	--overtemp <value>\n" <<
		"		delta from the fire temperature to iterate from: default " << overTemp << "\n" <<
		"	--loglevel <value>\n" <<
		"		what level of the nested loop to log, 0-6: none, [default] globalBest, thirTemp, fuelTemp, targetTemp, all, debug\n" <<
		"	--param\n" <<
		"		lets you configure what and how to optimise\n" <<
		"	--restrict\n" <<
		"		lets you make atmosim not consider bombs outside of chosen parameters\n" <<
		"ss14 maxcap atmos sim" << endl;
}

int main(int argc, char* argv[]) {
	// setup
	setupParams();

	string gas1, gas2, gas3;
	float mixt1, mixt2, thirt1, thirt2;

	// args parsing
	if (argc > 1) {
		for (int i = 0; i < argc; i++) {
			int more = i+1 < argc;
			string arg(argv[i]);
			if (arg[0] != '-' || arg.length() < 2) {
				continue;
			}
			if (arg[1] == '-') {
				if (arg.rfind("--help", 0) == 0) {
					showHelp();
					return 0;
				} else if (arg.rfind("--gas1", 0) == 0 && more) {
					gas1 = string(argv[++i]);
				} else if (arg.rfind("--gas2", 0) == 0 && more) {
					gas2 = string(argv[++i]);
				} else if (arg.rfind("--gas3", 0) == 0 && more) {
					gas3 = string(argv[++i]);
				} else if (arg.rfind("--mixt1", 0) == 0 && more) {
					mixt1 = std::stod(argv[++i]);
				} else if (arg.rfind("--mixt2", 0) == 0 && more) {
					mixt2 = std::stod(argv[++i]);
				} else if (arg.rfind("--thirt1", 0) == 0 && more) {
					thirt1 = std::stod(argv[++i]);
				} else if (arg.rfind("--thirt2", 0) == 0 && more) {
					thirt2 = std::stod(argv[++i]);
				} else if (arg.rfind("--ticks", 0) == 0 && more) {
					tickCap = std::stoi(argv[++i]);
				} else if (arg.rfind("--tstep", 0) == 0 && more) {
					temperatureStep = std::stod(argv[++i]);
                } else if (arg.rfind("--tstepm", 0) == 0 && more) {
					temperatureStepMin = std::stod(argv[++i]);
				} else if (arg.rfind("--volume", 0) == 0 && more) {
					volume = std::stod(argv[++i]);
				} else if (arg.rfind("--pressureCap", 0) == 0 && more) {
					pressureCap = std::stod(argv[++i]);
				} else if (arg.rfind("--overtemp", 0) == 0 && more) {
					overTemp = std::stod(argv[++i]);
				} else if (arg.rfind("--loglevel", 0) == 0 && more) {
					logLevel = std::stoi(argv[++i]);
				} else if (arg.rfind("--param", 0) == 0) {
					string optimiseWhat = "";
					cout << "Possible optimisations: " << listParams() << endl;
					cout << "Enter what to optimise: ";
					cin >> optimiseWhat;
					DynVal optVal = getParam(optimiseWhat);
					if (!optVal.valid()) {
						cout << "Invalid optimisation parameter." << endl;
						continue;
					}
					optimiseVal = optVal;
					string doMaximise;
					cout << "Maximise? [Y/n]: ";
					cin >> doMaximise;
					optimiseMaximise = evalOpt(doMaximise);
					string doMeasureBefore;
					cout << "Measure stat before ignition? [y/N]: ";
					cin >> doMeasureBefore;
					optimiseBefore = evalOpt(doMeasureBefore, false);
				} else if (arg.rfind("--restrict", 0) == 0) {
					while (true) {
						string restrictWhat = "";
						cout << "Available parameters: " << listParams() << endl;
						cout << "Enter -1 as the upper limit on numerical restrictions to have no limit." << endl;
						cout << "Enter what to restrict: ";
						cin >> restrictWhat;
						bool valid = false;
						DynVal optVal = getParam(restrictWhat);
						BaseRestriction* restrict;
						switch (optVal.type) {
							case (IntVal): {
								auto [minv, maxv] = getMinMaxLimit<int>();
								restrict = new NumRestriction<int>(getDynPtr<int>(optVal), minv, maxv);
								valid = true;
								break;
							}
							case (FloatVal): {
								auto [minv, maxv] = getMinMaxLimit<float>();
								restrict = new NumRestriction<float>(getDynPtr<float>(optVal), minv, maxv);
								valid = true;
								break;
							}
							case (BoolVal): {
								string tgt;
								cout << "Enter target value: [Y/n] ";
								cin >> tgt;
								restrict = new BoolRestriction(getDynPtr<bool>(optVal), evalOpt(tgt));
								valid = true;
								break;
							}
							default: {
								cout << "Invalid parameter." << endl;
								break;
							}
						}
						if (valid) {
							string resAfter;
							cout << "Restrict after simulation done? [Y/n] ";
							cin >> resAfter;
							if (evalOpt(resAfter)) {
								postRestrictions.push_back(restrict);
							} else {
								preRestrictions.push_back(restrict);
							}
						}
						string cont;
						cout << "Continue? [y/N] ";
						cin >> cont;
						if (!evalOpt(cont, false)) {
							break;
						}
					}
				} else {
					cout << "Unrecognized argument '" << arg << "'." << endl;
					showHelp();
					return 0;
				}
				continue;
			}
			switch (arg[1]) {
				case 'n': {
					checkStatus = !checkStatus;
					break;
				}
				case 'H': {
					heatCapInputSetup();
					break;
				}
				case 'r': {
					cout << "max gas1:gas2: ";
					cin >> ratioFrom;
					cout << "max gas2:gas1: ";
					cin >> ratioTo;
					cout << "ratio step: ";
					cin >> ratioStep;
					break;
				}
				case 's': {
					stepTargetTemp = !stepTargetTemp;
					break;
				}
				case 'm': {
					float t1, t2, ratio, capratio;
					cout << "temp1: ";
					cin >> t1;
					cout << "temp2: ";
					cin >> t2;
					cout << "molar ratio (first%): ";
					cin >> ratio;
					cout << "second:first heat capacity ratio (omit if end temperature does not matter): ";
					cin >> capratio;
					ratio = 100.0 / ratio - 1.0;
					cout << "pressure ratio: " << 100.0 / (1.0 + ratio * t2 / t1) << "% first | temp " << (t1 + t2 * capratio * ratio) / (1.0 + capratio * ratio) << "K";
					return 0;
				}
				case 'f': {
					fullInputSetup();
					loop();
					status();
					return 0;
				}
				case 'h': {
					showHelp();
					return 0;
				}
				default: {
					cout << "Unrecognized argument '" << arg << "'." << endl;
					showHelp();
					break;
				}
			}
		}
	}
	// didn't exit prior, test 1 gas -> 2-gas-mix tanks
	cout << "Gases: " << listGases() << endl;
	if (gas1.length() == 0) {
		cout << "first mix gas: ";
		cin >> gas1;
	}
	if (gas2.length() == 0) {
		cout << "second mix gas: ";
		cin >> gas2;
	}
	if (gas3.length() == 0) {
		cout << "inserted gas: ";
		cin >> gas3;
	}
    selectedGases[0] = gas1;
    selectedGases[1] = gas2;
    selectedGases[2] = gas3;

	if (!mixt1) {
		cout << "mix temp min: ";
		cin >> mixt1;
	}
	if (!mixt2) {
		cout << "mix temp max: ";
		cin >> mixt2;
	}
	if (!thirt1) {
		cout << "inserted temp min: ";
		cin >> thirt1;
	}
	if (!thirt2) {
		cout << "inserted temp max: ";
		cin >> thirt2;
	}

	BombData bestBomb = testTwomix(sToG(gas1), sToG(gas2), sToG(gas3), mixt1, mixt2, thirt1, thirt2, optimiseMaximise, optimiseBefore);
	cout << "Best:\n" << bestBomb.printExtensive() << endl;
    cout << "Retest and print ticks? [y/N]: ";
    string doRetest;
    cin >> doRetest;
    if (evalOpt(doRetest, false)) {
        reset();
        knownInputSetup(sToG(gas1), sToG(gas2), sToG(gas3), bestBomb.fuelTemp, bestBomb.thirTemp, bestBomb.fuelPressure, bestBomb.ratio);
        loopPrint();
    }
	return 0;
}

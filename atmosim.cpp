#ifdef PLOT
#include <sciplot/sciplot.hpp>
#endif

#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

enum ValueType {IntVal, FloatVal, BoolVal, NoneVal};

struct DynVal {
	ValueType type;
	void* valuePtr;

	bool invalid() {
		return type == NoneVal || valuePtr == nullptr;
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

	NumRestriction(T* ptr, T min, T max): valuePtr(ptr), minValue(min), maxValue(max) {
		if (maxValue < 0) {
			maxValue = numeric_limits<T>::max();
		}
	}

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


const int gas_count = 8;
float gasAmounts[gas_count]{};
float gasHeatCaps[gas_count]{20.f * heatScale, 30.f * heatScale, 200.f * heatScale, 10.f * heatScale, 40.f * heatScale, 30.f * heatScale, 600.f * heatScale, 40.f * heatScale};
const string gasNames[gas_count]{  "oxygen",         "nitrogen",       "plasma",          "tritium",        "waterVapour",    "carbonDioxide",  "frezon",          "nitrousOxide"  };

const int invalid_gas_num = -1;

// integer container struct denoting a gas type
struct GasType {
	int gas = invalid_gas_num;

	float& amount() const {
		return gasAmounts[gas];
	}

	void updateAmount(const float& delta, float& heatCapacityCache) {
		amount() += delta;
		heatCapacityCache += delta * heatCap();
	}

	float& heatCap() const {
		return gasHeatCaps[gas];
	}

	bool invalid() const {
		return gas == invalid_gas_num;
	}

	string name() const {
		return gasNames[gas];
	}

	bool operator== (const GasType& other) {
		return gas == other.gas;
	}

	bool operator!= (const GasType& other) {
		return gas != other.gas;
	}
};

GasType oxygen{0};
GasType nitrogen{1};
GasType plasma{2};
GasType tritium{3};
GasType waterVapour{4};
GasType carbonDioxide{5};
GasType frezon{6};
GasType nitrousOxide{7};
GasType invalidGas{invalid_gas_num};

GasType gases[]{oxygen, nitrogen, plasma, tritium, waterVapour, carbonDioxide, frezon, nitrousOxide};

unordered_map<string, GasType> gasMap{
	{"oxygen",        oxygen       },
	{"nitrogen",      nitrogen     },
	{"plasma",        plasma       },
	{"tritium",       tritium      },
	{"waterVapour",   waterVapour  },
	{"carbonDioxide", carbonDioxide},
	{"frezon",        frezon       },
	{"nitrousOxide",  nitrousOxide }};

string listGases() {
	string out;
	for (GasType g : gases) {
		out += g.name() + ", ";
	}
	out.resize(out.size() - 2);
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
simpleOutput = false, silent = false,
optimiseInt = false, optimiseMaximise = true, optimiseBefore = false;
float fireTemp = 373.15, minimumHeatCapacity = 0.0003, oneAtmosphere = 101.325, R = 8.314462618,
tankLeakPressure = 30.0 * oneAtmosphere, tankRupturePressure = 40.0 * oneAtmosphere, tankFragmentPressure = 50.0 * oneAtmosphere, tankFragmentScale = 2.0 * oneAtmosphere,
fireHydrogenEnergyReleased = 284000.0 * heatScale, minimumTritiumOxyburnEnergy = 143000.0, tritiumBurnOxyFactor = 100.0, tritiumBurnTritFactor = 10.0,
firePlasmaEnergyReleased = 160000.0 * heatScale, superSaturationThreshold = 96.0, superSaturationEnds = superSaturationThreshold / 3.0, oxygenBurnRateBase = 1.4, plasmaUpperTemperature = 1643.15, plasmaOxygenFullburn = 10.0, plasmaBurnRateDelta = 9.0,
n2oDecompTemp = 850.0, N2ODecompositionRate = 0.5,
frezonCoolTemp = 23.15, frezonCoolLowerTemperature = 23.15, frezonCoolMidTemperature = 373.15, frezonCoolMaximumEnergyModifier = 10.0, frezonCoolRateModifier = 20.0, frezonNitrogenCoolRatio = 5.0, frezonCoolEnergyReleased = -600000.0 * heatScale,
tickrate = 0.5,
overTemp = 0.1, temperatureStep = 1.002, temperatureStepMin = 0.1, ratioStep = 1.005, ratioFrom = 10.0, ratioTo = 10.0, amplifScale = 1.2, amplifDownscale = 1.4, maxAmplif = 20.0, maxDeriv = 1.005,
heatCapacityCache = 0.0;
vector<GasType> activeGases;
string rotator = "|/-\\";
int rotatorChars = 4;
int rotatorIndex = rotatorChars - 1;
long long progressBarSpacing = 4817;
// ETA values are in ms
const long long ProgressUpdateSpacing = progressBarSpacing * 25;
const int ProgressPolls = 20;
const long long ProgressPollWindow = ProgressUpdateSpacing * ProgressPolls;
long long ProgressPollTimes[ProgressPolls];
long long ProgressPoll = 0;
long long lastSpeed = 0;
chrono::high_resolution_clock mainClock;

DynVal optimiseVal = {FloatVal, &radius};
vector<BaseRestriction*> preRestrictions;
vector<BaseRestriction*> postRestrictions;

bool restrictionsMet(const vector<BaseRestriction*>& restrictions) {
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
	{"",            {NoneVal,  nullptr     }},
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

DynVal getParam(const string& name) {
	if (simParams.contains(name)) {
		return simParams[name];
	}
	return simParams[""];
}

DynVal& operator>> (istream& stream, DynVal& param) {
	string val;
	stream >> val;
	param = getParam(val);
	if (param.invalid()) {
		cin.setstate(ios_base::failbit);
	}
	return param;
}

// flushes a basic_istream<char> until after \n
basic_istream<char>& flush_stream(basic_istream<char>& stream) {
	stream.ignore(numeric_limits<streamsize>::max(), '\n');
	return stream;
}

// query user input from keyboard, ask again if invalid
template <typename T>
T getInput(const string& what, const string& invalid_err = "Invalid value. Try again.\n") {
	bool valid = false;
	T val;
	while (!valid) {
		valid = true;
		cout << what;
		cin >> val;
		if (cin.fail() || cin.peek() != '\n') {
			cerr << invalid_err;
			cin.clear();
			flush_stream(cin);
			valid = false;
		}
	}
	return val;
}

// returns true if user entered nothing, false otherwise
bool await_input() {
	return flush_stream(cin).peek() == '\n';
}

// evaluates a string as an [y/n] option
bool evalOpt(const string& opt, bool default_opt = true) {
	return opt == "y" || opt == "Y" // is it Y?
	||    (opt != "n" && opt != "N" && default_opt); // it's not Y, so check if it's not N, and if so, return default
}

// requests an [y/n] input from user
bool getOpt(const string& what, bool default_opt = true) {
	cout << what << (default_opt ? " [Y/n] " : " [y/N] ");

	if (await_input()) return default_opt; // did the user just press enter?

	string opt; // we have non-empty input so check what it is
	cin >> opt;
	return evalOpt(opt, default_opt);
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

bool isGas(const string& gas) {
	return gasMap.contains(gas);
}

// string-to-gas
GasType sToG(const string& gas) {
	if (!isGas(gas)) {
		return invalidGas;
	}
	return gasMap[gas];
}

// string-to-gas but throw an exception if invalid
GasType parseGas(const string& gas) {
	GasType out = sToG(gas);
	if (out.invalid()) {
		throw invalid_argument("Parsed invalid gas type.");
	}
	return out;
}

istream& operator>> (istream& stream, GasType& g) {
	string val;
	stream >> val;
	g = sToG(val);
	if (g == invalidGas) {
		cin.setstate(ios_base::failbit);
	}
	return stream;
}

float getHeatCapacity() {
	float sum = 0.0;
	for (const GasType& g : gases) {
		sum += g.amount() * g.heatCap();
	}
	return sum;
}
void updateHeatCapacity(const GasType& type, const float& molesDelta, float& capacity) {
	capacity += type.heatCap() * molesDelta;
}
float getGasMols() {
	float sum = 0.0;
	for (const GasType& g : gases) {
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

void doPlasmaFire() {
	float oldHeatCapacity = heatCapacityCache;
	float energyReleased = 0.0;
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

			plasma.updateAmount(-plasmaBurnRate, heatCapacityCache);

			oxygen.updateAmount(-plasmaBurnRate * oxygenBurnRate, heatCapacityCache);

			float tritDelta = plasmaBurnRate * supersaturation;
			tritium.updateAmount(tritDelta, heatCapacityCache);

			float carbonDelta = plasmaBurnRate - tritDelta;
			carbonDioxide.updateAmount(carbonDelta, heatCapacityCache);

			energyReleased += firePlasmaEnergyReleased * plasmaBurnRate;
		}
	}
	if (heatCapacityCache > minimumHeatCapacity) {
		temperature = (temperature * oldHeatCapacity + energyReleased) / heatCapacityCache;
	}
}
void doTritFire() {
	float oldHeatCapacity = heatCapacityCache;
	float energyReleased = 0.f;
	float burnedFuel = 0.f;
	if (oxygen.amount() < tritium.amount() || minimumTritiumOxyburnEnergy > temperature * heatCapacityCache) {
		burnedFuel = std::min(tritium.amount(), oxygen.amount() / tritiumBurnOxyFactor);
		float tritDelta = -burnedFuel;
		tritium.updateAmount(tritDelta, heatCapacityCache);
	} else {
		burnedFuel = tritium.amount();
		float tritDelta = -tritium.amount() / tritiumBurnTritFactor;

		tritium.updateAmount(tritDelta, heatCapacityCache);
		oxygen.updateAmount(-tritium.amount(), heatCapacityCache);

		energyReleased += fireHydrogenEnergyReleased * burnedFuel * (tritiumBurnTritFactor - 1.f);
	}
	if (burnedFuel > 0.f) {
		energyReleased += fireHydrogenEnergyReleased * burnedFuel;

		waterVapour.updateAmount(burnedFuel, heatCapacityCache);
	}
	if (heatCapacityCache > minimumHeatCapacity) {
		temperature = (temperature * oldHeatCapacity + energyReleased) / heatCapacityCache;
	}
}
void doN2ODecomposition() {
	float oldHeatCapacity = heatCapacityCache;
	float& n2o = nitrousOxide.amount();
	float burnedFuel = n2o * N2ODecompositionRate;
	nitrousOxide.updateAmount(-burnedFuel, heatCapacityCache);
	nitrogen.updateAmount(burnedFuel, heatCapacityCache);
	oxygen.updateAmount(burnedFuel * 0.5f, heatCapacityCache);
	temperature *= oldHeatCapacity / heatCapacityCache;
}
void doFrezonCoolant() {
	float oldHeatCapacity = heatCapacityCache;
	float energyModifier = 1.f;
	float scale = (temperature - frezonCoolLowerTemperature) / (frezonCoolMidTemperature - frezonCoolLowerTemperature);
	if (scale > 1.f) {
		energyModifier = std::min(scale, frezonCoolMaximumEnergyModifier);
		scale = 1.f;
	}
	float burnRate = frezon.amount() * scale / frezonCoolRateModifier;
	float energyReleased = 0.f;
	if (burnRate > minimumHeatCapacity) {
		float nitDelta = -std::min(burnRate * frezonNitrogenCoolRatio, nitrogen.amount());
		float frezonDelta = -std::min(burnRate, frezon.amount());

		nitrogen.updateAmount(nitDelta, heatCapacityCache);
		frezon.updateAmount(frezonDelta, heatCapacityCache);
		nitrousOxide.updateAmount(-nitDelta - frezonDelta, heatCapacityCache);

		energyReleased = burnRate * frezonCoolEnergyReleased * energyModifier;
	}
	if (heatCapacityCache > minimumHeatCapacity) {
		temperature = (temperature * oldHeatCapacity + energyReleased) / heatCapacityCache;
	}
}

void react() {
	heatCapacityCache = getHeatCapacity();
	if (temperature >= frezonCoolTemp && nitrogen.amount() >= 0.01f && frezon.amount() >= 0.01f) {
		doFrezonCoolant();
	}
	if (temperature >= n2oDecompTemp && nitrousOxide.amount() >= 0.01f) {
		doN2ODecomposition();
	}
	if (temperature >= fireTemp && oxygen.amount() >= 0.01f) {
		if (tritium.amount() >= 0.01f) {
			doTritFire();
		}
		if (plasma.amount() >= 0.01f) {
			doPlasmaFire();
		}
	}
}
void tankCheckStatus() {
	float pressure = getPressure();
	if (pressure > tankLeakPressure) {
		if (pressure > tankRupturePressure) {
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
	cout << "TICK: " << tick << " || Status: pressure " << getPressure() << "kPa \\ integrity " << integrity << " \\ temperature " << temperature << "K\nContents: ";
	for (GasType g : gases) {
		cout << g.name() << ": " << g.amount() << " mol; ";
	}
	cout << endl;
	if (tankState == exploded) {
		cout << "EXPLOSION: range " << getCurRange() << endl;
	} else if (tankState == ruptured) {
		cout << "RUPTURED" << endl;
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
	float sumheat = 0.0;
	while (true) {
		cout << "Available gases: " << listGases() << endl;
		GasType gas = getInput<GasType>("Enter gas to add: ");
		float moles = getInput<float>("Enter moles: ");
		float temp = getInput<float>("Enter temperature: ");
		sumheat += temp * gas.heatCap() * moles;
		gas.amount() += moles;
		if (!getOpt("Continue?")) {
			break;
		}
	}
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
	float ratio, fuelTemp, fuelPressure, thirTemp, mixPressure, mixTemp;
	GasType gas1, gas2, gas3;
	TankState state = intact;
	float radius = 0.0, finTemp = -1.0, finPressure = -1.0, finHeatLeak = -1.0, optstat = -1.0;
	int ticks = -1;

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

	string printVerySimple() const {
		float firstFraction = 1.f / (1.f + ratio);
		return string(to_string(fuelTemp) + " " + to_string(fuelPressure) + " " + to_string(firstFraction) + " " + to_string(thirTemp));
	}

	string printSimple() const {
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
			"optstat: " + to_string(optstat) + " " +
		"}";
	}

	string printExtensive() const {
		float firstFraction = 1.f / (1.f + ratio);
		float secondFraction = ratio * firstFraction;
		float volumeRatio = (requiredTransferVolume + volume) / volume;
		float addedRatio = (requiredTransferVolume + volume) / requiredTransferVolume;
		return string(
		"TANK: {\n" ) +
			"\tinitial state: [\n" +
				"\t\ttemperature\t" + to_string(mixTemp) + " K\n" +
				"\t\tpressure\t" + to_string(mixPressure) + " kPa\n" +
				"\t\t" + gas1.name() + to_string(pressureTempToMols(firstFraction * fuelPressure, fuelTemp)) + " mol\t" + "\n" +
				"\t\t" + gas2.name() + to_string(pressureTempToMols(secondFraction * fuelPressure, fuelTemp)) + " mol\t" + "\n" +
				"\t\t" + gas3.name() + to_string(pressureTempToMols(pressureCap - fuelPressure, thirTemp)) + " mol\t" + "\n" +
			"\t];\n" +
			"\tend state: [\n" +
				"\t\ttime\t\t" + to_string(ticks * tickrate) + " s\n" +
				"\t\tpressure \t" + to_string(finPressure) + " kPa\n" +
				"\t\ttemperature\t" + to_string(finTemp) + " K\n" +
				"\t\t" + (
				state == exploded ?
				"explosion\t" + to_string(radius) + " tiles "
				: state == ruptured ? "ruptured" : "no explosion" ) + "\n" +
			"\t]\n" +
			"\toptstat\t" + to_string(optstat) + "\n" +
		"};\n" +
		"REQUIREMENTS: {\n" +
			"\tmix-canister (fuel): [\n" +
				"\t\tgas ratio\t" + gas1.name() + "\t\t" + gas2.name() + "\n" +
				"\t\tgas ratio\t" + to_string(100.f * firstFraction) + "%\t" + to_string(100.f * secondFraction) + "%\n" +
				"\t\tgas ratio\t" + to_string(1.0/ratio) + "\n" +
				"\t\ttemperature\t" + to_string(fuelTemp) + " K\n" +
				"\t\ttank pressure\t" + to_string(fuelPressure) + " kPa\n" +
				"\t\tleast-mols: [\n" +
					"\t\t\t" + gas1.name() + "\t\t" + gas2.name() + "\n" +
					"\t\t\t" + to_string(pressureTempToMols(firstFraction * fuelPressure, fuelTemp) * volumeRatio) + "\t" + to_string(pressureTempToMols(secondFraction * fuelPressure, fuelTemp) * volumeRatio) + "\n" +
				"\t\t]\n"
			"\t];\n" +
			"\tthird-canister (primer): [\n" +
				"\t\ttemperature\t" + to_string(thirTemp) + " K\n" +
				"\t\tpressure\t" + to_string((pressureCap * 2.0 - fuelPressure) * addedRatio) + " kPa\n" +
				"\t\tleast-mols:\t" + to_string(pressureTempToMols(pressureCap * 2.0 - fuelPressure, thirTemp) * volumeRatio) + " mol\t" + gas3.name() + "\n" +
			"\t]\n" +
		"}\n" +
		"REVERSE-REQUIREMENTS: {\n" +
			"\tmix-canister (primer): [\n" +
				"\t\tgas ratio\t" + gas1.name() + "\t\t" + gas2.name() + "\n" +
				"\t\tgas ratio\t" + to_string(100.f * firstFraction) + "%\t" + to_string(100.f * secondFraction) + "%\n" +
				"\t\tgas ratio\t" + to_string(1.0/ratio) + "\n" +
				"\t\ttemperature\t" + to_string(fuelTemp) + " K\n" +
				"\t\tpressure\t" + to_string((pressureCap + fuelPressure) * addedRatio) + " kPa\n" +
				"\t\tleast-mols: [\n" +
					"\t\t\t" + gas1.name() + "\t\t" + gas2.name() + "\n" +
					"\t\t\t" + to_string(pressureTempToMols(pressureCap + fuelPressure, fuelTemp) * firstFraction * volumeRatio) + "\t" + to_string(pressureTempToMols(pressureCap + fuelPressure, fuelTemp) * secondFraction * volumeRatio) + "\n" +
				"\t\t];\n" +
			"\t];\n" +
			"\tthird-canister (fuel): [\n" +
				"\t\ttemperature\t" + to_string(thirTemp) + " K\n" +
				"\t\ttank pressure\t" + to_string((pressureCap - fuelPressure)) + " kPa\n" +
				"\t\tleast-mols:\t" + to_string(pressureTempToMols(pressureCap - fuelPressure, thirTemp) * volumeRatio) + " mol\t" + gas3.name() + "\n" +
			"\t]\n" +
		"}";
	}
};

void printBomb(const BombData& bomb, const string& what, bool extensive = false) {
	cout << what << (simpleOutput ? bomb.printVerySimple() : (extensive ? bomb.printExtensive() : bomb.printSimple())) << endl;
}
string getProgressBar(long progress, long size) {
	string progressBar = '[' + string(progress, '#') + string(size - progress, ' ') + ']';
	return progressBar;
}
void printProgress(long long iters, auto startTime) {
	printf("%lli Iterations %c ", iters, getRotator());
	if (iters % ProgressUpdateSpacing == 0) {
		long long curTime = chrono::duration_cast<chrono::milliseconds>(mainClock.now() - startTime).count();
		ProgressPollTimes[ProgressPoll] = curTime;
		ProgressPoll = (ProgressPoll + 1) % ProgressPolls;
		long long pollTime = ProgressPollTimes[ProgressPoll];
		long long timePassed = curTime - pollTime;
		float progressPassed = std::min(ProgressPollWindow, iters);
		lastSpeed = (float)progressPassed / timePassed * 1000.f;
	}
	printf("[Speed: %lli iters/s]\r", lastSpeed);
	cout.flush();
}


#ifdef PLOT
void plotCurrent(float stats[], vector<float> xVals[], vector<float> yVals[], float curValue, const int i) {
	float stat = stats[i];
	xVals[i].push_back(curValue);
	yVals[i].push_back(stat);
}
void checkResetPlot(vector<float> xVals[], vector<float> yVals[], vector<float> tempXVals[], vector<float> tempYVals[], float globalBestStats[], float lastBestStats[], const int i) {
	if (globalBestStats[i] != lastBestStats[i]) {
		xVals[i] = tempXVals[i];
		yVals[i] = tempYVals[i];
		lastBestStats[i] = globalBestStats[i];
	}
	tempXVals[i].clear();
	tempYVals[i].clear();
}
#endif

float optimiseStat() {
	return optimiseVal.type == FloatVal ? getDyn<float>(optimiseVal) : getDyn<int>(optimiseVal);
}
void updateAmplif(float lastStats[], float amplifs[], float stats[], const int i, bool maximise) {
	float stat = stats[i];
	float deriv = stat / lastStats[i];
	float absDeriv = maximise ? deriv : 1.f / deriv;
	float& amplif = amplifs[i];
	amplif = std::max(1.f, amplif * (absDeriv > maxDeriv && absDeriv == absDeriv ? 1.f / (absDeriv / maxDeriv) / amplifDownscale : amplifScale));
	amplif = std::min(amplif, maxAmplif);
	lastStats[i] = stat;
	stats[i] = maximise ? numeric_limits<float>::min() : numeric_limits<float>::max();
}
BombData testTwomix(GasType gas1, GasType gas2, GasType gas3, float mixt1, float mixt2, float thirt1, float thirt2, bool maximise, bool measureBefore) {
	// parameters of the tank with the best result we have so far
	BombData bestBomb(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, gas1, gas2, gas3);
	bestBomb.optstat = maximise ? numeric_limits<float>::min() : numeric_limits<float>::max();

	// same but only best in the current surrounding frame
	BombData bestBombLocal(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, gas1, gas2, gas3);
	bestBombLocal.optstat = maximise ? numeric_limits<float>::min() : numeric_limits<float>::max();

	#ifdef PLOT
	sciplot::Plot2D plot1, plot2, plot3, plot4;
	vector<float> xVals[4];
	vector<float> yVals[4];
	vector<float> xValsTemp[4];
	vector<float> yValsTemp[4];
	float globalBestStats[4] {1.f, 1.f, 1.f, 1.f};
	float lastBestStats[4] {1.f, 1.f, 1.f, 1.f};
	#endif

	long long iters = 0;
	chrono::time_point startTime = mainClock.now();
	float lastStats[4] {1.f, 1.f, 1.f, 1.f};
	float amplifs[4] {1.f, 1.f, 1.f, 1.f};
	float bestStats[4] {1.f, 1.f, 1.f, 1.f};
	for (float thirTemp = thirt1; thirTemp <= thirt2; thirTemp = std::max(thirTemp * (1.f + (temperatureStep - 1.f) * amplifs[0]), thirTemp + temperatureStepMin * amplifs[0])) {
		for (float fuelTemp = mixt1; fuelTemp <= mixt2; fuelTemp = std::max(fuelTemp * (1.f + (temperatureStep - 1.f) * amplifs[1]), fuelTemp + temperatureStepMin * amplifs[1])) {
			float targetTemp2 = stepTargetTemp ? std::max(thirTemp, fuelTemp) : fireTemp + overTemp + temperatureStep;
			for (float targetTemp = fireTemp + overTemp; targetTemp < targetTemp2; targetTemp = std::max(targetTemp * (1.f + (temperatureStep - 1.f) * amplifs[2]), targetTemp + temperatureStepMin * amplifs[2])) {
				for (float ratio = 1.0 / ratioFrom; ratio <= ratioTo; ratio += ratio * (ratioStep - 1.f) * amplifs[3]) {
					++iters;
					if (iters % progressBarSpacing == 0) {
						printProgress(iters, startTime);
					}
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
						printBomb(curBomb, "\n", true);
					}
					if (noDiscard && (maximise == (stat > bestBombLocal.optstat))) {
						bestBombLocal = curBomb;
					}
					for (float& s : bestStats) {
						if (noDiscard && (maximise == (stat > s))) {
							s = stat;
						}
					}
					#ifdef PLOT
					for (float& s : globalBestStats) {
						if (noDiscard && (maximise == (stat > s))) {
							s = stat;
						}
					}
					plotCurrent(bestStats, xValsTemp, yValsTemp, ratio, 3);
					#endif
					updateAmplif(lastStats, amplifs, bestStats, 3, maximise);
				}
				#ifdef PLOT
				checkResetPlot(xVals, yVals, xValsTemp, yValsTemp, globalBestStats, lastBestStats, 3);
				plotCurrent(bestStats, xValsTemp, yValsTemp, targetTemp, 2);
				#endif
				updateAmplif(lastStats, amplifs, bestStats, 2, maximise);
				if (logLevel == 4) {
					printBomb(bestBombLocal, "Current: ");
					bestBombLocal.optstat = maximise ? numeric_limits<float>::min() : numeric_limits<float>::max();
				}
			}
			#ifdef PLOT
			checkResetPlot(xVals, yVals, xValsTemp, yValsTemp, globalBestStats, lastBestStats, 2);
			plotCurrent(bestStats, xValsTemp, yValsTemp, fuelTemp, 1);
			#endif
			updateAmplif(lastStats, amplifs, bestStats, 1, maximise);
			if (logLevel == 3) {
				printBomb(bestBombLocal, "Current: ");
				bestBombLocal.optstat = maximise ? numeric_limits<float>::min() : numeric_limits<float>::max();
			}
		}
		#ifdef PLOT
		checkResetPlot(xVals, yVals, xValsTemp, yValsTemp, globalBestStats, lastBestStats, 1);
		plotCurrent(bestStats, xVals, yVals, thirTemp, 0);
		#endif
		updateAmplif(lastStats, amplifs, bestStats, 0, maximise);
		if (logLevel == 2) {
			printBomb(bestBombLocal, "Current: ");
			bestBombLocal.optstat = maximise ? numeric_limits<float>::min() : numeric_limits<float>::max();
		} else if (logLevel == 1) {
			printBomb(bestBombLocal, "Best: ");
		}
	}
	#ifdef PLOT
	plot4.drawCurve(xVals[3], yVals[3]).label("ratio->optstat");
	plot4.xtics().logscale(2);
	plot3.drawCurve(xVals[2], yVals[2]).label("targetTemp->optstat");
	plot2.drawCurve(xVals[1], yVals[1]).label("fuelTemp->optstat");
	plot1.drawCurve(xVals[0], yVals[0]).label("thirTemp->optstat");
	sciplot::Figure fig = {{plot1, plot2}, {plot3, plot4}};
	sciplot::Canvas canv = {{fig}};
	canv.size(900, 900);
	canv.show();
	#endif
    return bestBomb;
}

void heatCapInputSetup() {
	cout << "Enter heat capacities for " << listGases() << ": ";
	for (GasType g : gases) {
		cin >> g.heatCap();
	};
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
		"		try full input: lets you manually input and a tank's contents and see what it does\n" <<
		"	--gas1 <value>\n" <<
		"		the type of the first gas in the mix gas (usually fuel, in tank)\n" <<
		"	--gas2 <value>\n" <<
		"		the type of the second gas in the mix gas (usually fuel, in tank)\n" <<
		"	--gas3 <value>\n" <<
		"		the type of the third gas (usually primer, goes into tank to detonate)\n" <<
		"	--mixt1 <value>\n" <<
		"		the minimum of the temperature range to check for the mix gas\n" <<
		"		temperatures for this and the following options are in kelvin\n" <<
		"	--mixt2 <value>\n" <<
		"		the maximum of the temperature range to check for the mix gas\n" <<
		"	--thirt1 <value>\n" <<
		"		the minimum of the temperature range to check for the third gas\n" <<
		"	--thirt2 <value>\n" <<
		"		the maximum of the temperature range to check for the third gas\n" <<
		"	--doretest <y/N>\n" <<
		"		after calculating the bomb, whether to test it again and print every tick as it reacts\n" <<
		"	--ticks <value>\n" <<
		"		set tick limit: aborts if a bomb takes longer than this to detonate (default: " << tickCap << ")\n" <<
		"	--tstep <value>\n" <<
		"		set temperature iteration multiplier (default " << temperatureStep << ")\n" <<
		"	--tstepm <value>\n" <<
		"		set minimum temperature iteration step (default " << temperatureStepMin << ")\n" <<
		"	--volume <value>\n" <<
		"		set tank volume (default " << volume << ")\n" <<
		"	--overtemp <value>\n" <<
		"		only consider bombs which mix to this much above the ignition temperature; higher values may make bombs more robust to slight mismixing (default " << overTemp << ")\n" <<
		"	--loglevel <value>\n" <<
		"		what level of the nested loop to log, 0-6: none, [default] globalBest, thirTemp, fuelTemp, targetTemp, all, debug\n" <<
		"	--param\n" <<
		"		lets you configure what and how to optimise\n" <<
		"	--restrict\n" <<
		"		lets you make atmosim not consider bombs outside of chosen parameters\n" <<
		"	--simpleout\n" <<
		"		makes very simple output, for use by other programs or advanced users\n" <<
		"	--silent\n" <<
		"		output ONLY the final result, overrides loglevel\n" <<
		"ss14 maxcap atmos sim" << endl;
}

int main(int argc, char* argv[]) {
	// setup
	setupParams();

	GasType gas1, gas2, gas3;
	float mixt1 = 0.0, mixt2 = 0.0, thirt1 = 0.0, thirt2 = 0.0;
    string doRetest;

	// args parsing
	// TODO: nuke it all and rewrite in a sane way
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
					gas1 = parseGas(string(argv[++i]));
				} else if (arg.rfind("--gas2", 0) == 0 && more) {
					gas2 = parseGas(string(argv[++i]));
				} else if (arg.rfind("--gas3", 0) == 0 && more) {
					gas3 = parseGas(string(argv[++i]));
				} else if (arg.rfind("--mixt1", 0) == 0 && more) {
					mixt1 = std::stod(argv[++i]);
				} else if (arg.rfind("--mixt2", 0) == 0 && more) {
					mixt2 = std::stod(argv[++i]);
				} else if (arg.rfind("--thirt1", 0) == 0 && more) {
					thirt1 = std::stod(argv[++i]);
				} else if (arg.rfind("--thirt2", 0) == 0 && more) {
					thirt2 = std::stod(argv[++i]);
				} else if (arg.rfind("--doretest", 0) == 0 && more) {
					doRetest = string(argv[++i]);
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
				} else if (arg.rfind("--pressureCap", 0) == 0 && more) {
					pressureCap = std::stod(argv[++i]);
				} else if (arg.rfind("--amplifScale", 0) == 0 && more) {
					amplifScale = std::stod(argv[++i]);
					amplifDownscale = 1.f + 2.f * (amplifScale - 1.f);
				} else if (arg.rfind("--maxAmplif", 0) == 0 && more) {
					maxAmplif = std::stod(argv[++i]);
				} else if (arg.rfind("--maxDeriv", 0) == 0 && more) {
					maxDeriv = std::stod(argv[++i]);
				} else if (arg.rfind("--loglevel", 0) == 0 && more) {
					logLevel = std::stoi(argv[++i]);
				} else if (arg.rfind("--pspacing", 0) == 0 && more) {
					progressBarSpacing = std::stoi(argv[++i]);
				} else if (arg.rfind("--param", 0) == 0) {
					cout << "Possible optimisations: " << listParams() << endl;
					optimiseVal = getInput<DynVal>("Enter what to optimise: ");
					optimiseMaximise = getOpt("Maximise?");
					optimiseBefore = getOpt("Measure stat before ignition?", false);
				} else if (arg.rfind("--restrict", 0) == 0) {
					while (true) {
						string restrictWhat = "";
						cout << "Available parameters: " << listParams() << endl;
						cout << "Enter -1 as the upper limit on numerical restrictions to have no limit." << endl;
						DynVal optVal = getInput<DynVal>("Enter what to restrict: ");
						bool valid = false;
						BaseRestriction* restrict;
						switch (optVal.type) {
							case (IntVal): {
								int minv = getInput<int>("Enter lower limit: ");
								int maxv = getInput<int>("Enter upper limit: ");
								restrict = new NumRestriction<int>(getDynPtr<int>(optVal), minv, maxv);
								valid = true;
								break;
							}
							case (FloatVal): {
								float minv = getInput<float>("Enter lower limit: ");
								float maxv = getInput<float>("Enter upper limit: ");
								restrict = new NumRestriction<float>(getDynPtr<float>(optVal), minv, maxv);
								valid = true;
								break;
							}
							case (BoolVal): {
								restrict = new BoolRestriction(getDynPtr<bool>(optVal), getOpt("Enter target value:"));
								valid = true;
								break;
							}
							default: {
								cout << "Invalid parameter." << endl;
								break;
							}
						}
						if (valid) {
							if (getOpt("Restrict (Y=after | N=before) simulation done?")) {
								postRestrictions.push_back(restrict);
							} else {
								preRestrictions.push_back(restrict);
							}
						}
						if (!getOpt("Continue?", false)) {
							break;
						}
					}
				} else if (arg.rfind("--simpleout", 0) == 0) {
					simpleOutput = true;
				} else if (arg.rfind("--silent", 0) == 0) {
					silent = true;
				} else {
					cerr << "Unrecognized argument '" << arg << "'." << endl;
					showHelp();
					return 1;
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
					ratioFrom = getInput<float>("max gas1:gas2: ");
					ratioTo = getInput<float>("max gas2:gas1: ");
					ratioStep = getInput<float>("ratio step: ");
					break;
				}
				case 's': {
					stepTargetTemp = !stepTargetTemp;
					break;
				}
				case 'm': {
					float t1, t2, ratio, capratio;
					t1 = getInput<float>("temp1: ");
					t2 = getInput<float>("temp2: ");
					ratio = getInput<float>("molar ratio (first%): ");
					capratio = getInput<float>("second:first heat capacity ratio (omit if end temperature does not matter): ");
					
					ratio = 100.0 / ratio - 1.0;
					cout << "pressure ratio: " << 100.0 / (1.0 + ratio * t2 / t1) << "% first | temp " << (t1 + t2 * capratio * ratio) / (1.0 + capratio * ratio) << "K";
					
					return 0;
				}
				case 'f': {
					fullInputSetup();
					loopPrint();
					status();
					return 0;
				}
				case 'h': {
					showHelp();
					return 0;
				}
				default: {
					cerr << "Unrecognized argument '" << arg << "'." << endl;
					showHelp();
					break;
				}
			}
		}
	}
	if (silent) {
		// stop talking, be quiet for several days
		cout.setstate(ios::failbit);
	}
	// TODO: unhardcode parameter selection
	// didn't exit prior, test 1 gas -> 2-gas-mix tanks
	bool anyInvalid = gas1.invalid() || gas2.invalid() || gas3.invalid();
	if (anyInvalid && !silent) {
		cout << "Gases: " << listGases() << endl;
	}
	if (gas1.invalid()) {
		gas1 = getInput<GasType>("First gas of mix: ");
	}
	if (gas2.invalid()) {
		gas2 = getInput<GasType>("Second gas of mix: ");
	}
	if (gas3.invalid()) {
		gas3 = getInput<GasType>("Inserted gas: ");
	}

	if (!mixt1) {
		mixt1 = getInput<float>("mix temp min: ");
	}
	if (!mixt2) {
		mixt2 = getInput<float>("mix temp max: ");
	}
	if (!thirt1) {
		thirt1 = getInput<float>("inserted temp min: ");
	}
	if (!thirt2) {
		thirt2 = getInput<float>("inserted temp max: ");
	}

	BombData bestBomb = testTwomix(gas1, gas2, gas3, mixt1, mixt2, thirt1, thirt2, optimiseMaximise, optimiseBefore);
	cout.clear();
	cout << (simpleOutput ? "" : "Best:\n") << (simpleOutput ? bestBomb.printVerySimple() : bestBomb.printExtensive()) << endl;
	if (silent) {
		cout.setstate(ios::failbit);
	}
	bool retest;
	if (doRetest.length() == 0) {
		retest = getOpt("Retest and print ticks?", false);
	} else {
		retest = evalOpt(doRetest, false);
	}
    if (retest) {
        reset();
        knownInputSetup(gas1, gas2, gas3, bestBomb.fuelTemp, bestBomb.thirTemp, bestBomb.fuelPressure, bestBomb.ratio);
        loopPrint();
    }
	return 0;
}

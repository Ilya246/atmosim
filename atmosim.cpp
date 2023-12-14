#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <set>
#include <vector>

using namespace std;

struct GasType {
	float heatCap, amount;
};

float heatScale = 1.f / 8.f;

vector<GasType> gases{{20.f * heatScale, 0.0}, {200.f * heatScale, 0.0}, {10.f * heatScale, 0.0}, {40.f * heatScale, 0.0}, {30.f * heatScale, 0.0}, {600.f * heatScale, 0.0}};
GasType& oxygen = gases[0];
GasType& plasma = gases[1];
GasType& tritium = gases[2];
GasType& waterVapour = gases[3];
GasType& carbonDioxide = gases[4];
GasType& frezon = gases[5];
GasType none;
set<string> gasNames{"oxygen", "plasma", "tritium", "waterVapour", "carbonDioxide", "frezon"};

float temperature = 293.15, volume = 5.0, pressureCap = 1013.2, pipePressureCap = 4500.0,
scrubRate = 0.5,
radius = 0.0, // some stats here to be easily optimised for using the general-purpose method
leakedHeat = 0.0;
bool exploded = false;
int integrity = 3, leakCount = 0, tick = 0,
tickCap = 30, pipeTickCap = 1000,
logLevel = 1;
bool stepTargetTemp = false,
doScrub = false, checkStatus = true;
float fireTemp = 373.15, minimumHeatCapacity = 0.0003, oneAtmosphere = 101.325, R = 8.314462618,
tankLeakPressure = 30.0 * oneAtmosphere, tankRupturePressure = 40.0 * oneAtmosphere, tankFragmentPressure = 50.0 * oneAtmosphere, tankFragmentScale = 2.0 * oneAtmosphere,
fireHydrogenEnergyReleased = 284000.0, minimumTritiumOxyburnEnergy = 143000.0, tritiumBurnOxyFactor = 100.0, tritiumBurnTritFactor = 10.0,
firePlasmaEnergyReleased = 16000.0, superSaturationThreshold = 96.0, superSaturationEnds = superSaturationThreshold / 3.0, oxygenBurnRateBase = 1.4, plasmaUpperTemperature = 1643.15, plasmaOxygenFullburn = 10.0, plasmaBurnRateDelta = 9.0,
targetRadius = 0.0,
tickrate = 0.5,
overTemp = 0.1, temperatureStep = 1.005, ratioStep = 1.01, ratioFrom = 10.0, ratioTo = 10.0;
vector<GasType*> scrubGases;

void reset() {
	for (GasType& g : gases) {
		g.amount = 0.0;
	}
	temperature = 293.15;
	exploded = false;
	integrity = 3;
	tick = 0;
	leakCount = 0;
	radius = 0.0;
	leakedHeat = 0.0;
}

GasType& stog(string gas) {
	if (gas == "oxygen") {
		return oxygen;
	} else if (gas == "plasma") {
		return plasma;
	} else if (gas == "tritium") {
		return tritium;
	} else if (gas == "waterVapour") {
		return waterVapour;
	} else if (gas == "carbonDioxide") {
		return carbonDioxide;
	} else if (gas == "frezon") {
		return frezon;
	} else {
		cout << "Invalid gas." << endl;
		return none;
	}
}

bool isGas(string gas) {
	return gasNames.contains(gas);
}

float getHeatCapacity() {
	float sum = 0.0;
	for (GasType g : gases) {
		sum += g.amount * g.heatCap;
	}
	return sum;
}
float getGasMols() {
	float sum = 0.0;
	for (GasType g : gases) {
		sum += g.amount;
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
	return (gas1.amount * temp1 * gas1.heatCap + gas2.amount * temp2 * gas2.heatCap) / (gas1.amount * gas1.heatCap + gas2.amount * gas2.heatCap);
}
float mixGasTempsToTemp(float gasc1, float gashc1, float temp1, GasType gas2, float temp2) {
	return (gasc1 * temp1 * gashc1 + gas2.amount * temp2 * gas2.heatCap) / (gasc1 * gashc1 + gas2.amount * gas2.heatCap);
}
float getPressure() {
	return getGasMols() * R * temperature / volume;
}
float getCurRange() {
	return sqrt((getPressure() - tankFragmentPressure) / tankFragmentScale);
}

void checkDoPlasmaFire() {
	if (temperature < fireTemp) return;
	if (std::min(oxygen.amount, plasma.amount) < 0.01) return;
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
		float plasmaBurnRate = temperatureScale * (oxygen.amount > plasma.amount * plasmaOxygenFullburn ? plasma.amount / plasmaBurnRateDelta : oxygen.amount / plasmaOxygenFullburn / plasmaBurnRateDelta);
		if (plasmaBurnRate > minimumHeatCapacity) {
			plasmaBurnRate = std::min(plasmaBurnRate, std::min(plasma.amount, oxygen.amount / oxygenBurnRate));
			float supersaturation = std::min(1.0f, std::max((oxygen.amount / plasma.amount - superSaturationEnds) / (superSaturationThreshold - superSaturationEnds), 0.0f));
			plasma.amount -= plasmaBurnRate;
			oxygen.amount -= plasmaBurnRate * oxygenBurnRate;
			tritium.amount += plasmaBurnRate * supersaturation;
			carbonDioxide.amount += plasmaBurnRate * (1.0f - supersaturation);

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
	if (std::min(tritium.amount, oxygen.amount) < 0.01) return;
	float energyReleased = 0.0;
	float oldHeatCapacity = getHeatCapacity();
	float burnedFuel = 0.0;
	if (oxygen.amount < tritium.amount || minimumTritiumOxyburnEnergy > temperature * oldHeatCapacity) {
		burnedFuel = std::min(tritium.amount, oxygen.amount / tritiumBurnOxyFactor);
		tritium.amount -= burnedFuel;
	} else {
		burnedFuel = tritium.amount;
		tritium.amount *= 1.0 - 1.0 / tritiumBurnTritFactor;
		oxygen.amount -= tritium.amount;
		energyReleased += fireHydrogenEnergyReleased * burnedFuel * (tritiumBurnTritFactor - 1.0);
	}
	if (burnedFuel > 0.0) {
		energyReleased += fireHydrogenEnergyReleased * burnedFuel;
		waterVapour.amount += burnedFuel;
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
		for (int i = 0; i < 3; i++) {
			react();
		}
		exploded = true;
		radius = getCurRange();
		return;
	}
	if (pressure > tankRupturePressure) {
		if (integrity <= 0) {
			exploded = true;
			return;
		}
		integrity--;
		return;
	}
	if (pressure > tankLeakPressure) {
		if (integrity <= 0) {
			for (GasType& g : gases) {
				leakedHeat += g.amount * g.heatCap * temperature * 0.25;
				g.amount *= 0.75;
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
	if (!exploded) {
		cout << "No explosion" << endl;
	} else {
		cout << "EXPLOSION: range " << getCurRange() << endl;
	}
	cout << "Status: pressure " << getPressure() << "kPA integrity " << integrity << " temperature " << temperature << "K " << "tick " << tick << endl;
	cout << "Contents: " << oxygen.amount << " o2 " << plasma.amount << " p " << tritium.amount << " t " << waterVapour.amount << " w " << carbonDioxide.amount << " co2" << endl;
}

void scrub() {
	if (!doScrub) {
		return;
	}
	for (GasType* g : scrubGases) {
		g->amount *= scrubRate;
	}
}

void loop(int n) {
	while (tick < n) {
		react();
		scrub();
		++tick;
	}
}
void loop() {
	if (!checkStatus) {
		loop(tickCap);
		return;
	}
	while (tick < tickCap && !exploded) {
		react();
		tankCheckStatus();
		scrub();
		++tick;
	}
}


void fullInputSetup() {
	float tpressure, ttemp, sumheat;
	cout << "Oxygen kPA + K: " << endl;
	cin >> tpressure >> ttemp;
	oxygen.amount = pressureTempToMols(tpressure, ttemp);
	sumheat += oxygen.amount * oxygen.heatCap * ttemp;
	cout << "Plasma kPA + K: " << endl;
	cin >> tpressure >> ttemp;
	plasma.amount = pressureTempToMols(tpressure, ttemp);
	sumheat += plasma.amount * plasma.heatCap * ttemp;
	cout << "Trit kPA + K: " << endl;
	cin >> tpressure >> ttemp;
	tritium.amount = pressureTempToMols(tpressure, ttemp);
	sumheat += tritium.amount * tritium.heatCap * ttemp;
	cout << "frezon kPA + K: " << endl;
	cin >> tpressure >> ttemp;
	frezon.amount = pressureTempToMols(tpressure, ttemp);
	sumheat += frezon.amount * frezon.heatCap * ttemp;
	temperature = sumheat / getHeatCapacity();
}
float oxyplasmaInputSetup(float ptemp, float oxyTemp, float targetTemp) {
	float plasmaPressure = (targetTemp / oxyTemp - 1.0) * pressureCap / (plasma.heatCap / oxygen.heatCap - 1.0 + targetTemp * (1.0 / oxyTemp - plasma.heatCap / oxygen.heatCap / ptemp));
	plasma.amount = pressureTempToMols(plasmaPressure, ptemp);
	oxygen.amount = pressureTempToMols(pressureCap - plasmaPressure, oxyTemp);
	temperature = gasesTempsToTemp(plasma, ptemp, oxygen, oxyTemp);
	return plasmaPressure;
}
float mixInputSetup(GasType& gas1, GasType& gas2, GasType& into, float fuelTemp, float intoTemp, float targetTemp, float secondPerFirst) {
	float specheat = (gas1.heatCap + gas2.heatCap * secondPerFirst) / (1.0 + secondPerFirst);
	float fuelPressure = (targetTemp / intoTemp - 1.0) * pressureCap / (specheat / into.heatCap - 1.0 + targetTemp * (1.0 / intoTemp - specheat / into.heatCap / fuelTemp));
	float fuel = pressureTempToMols(fuelPressure, fuelTemp);
	gas1.amount = fuel / (1.0 + secondPerFirst);
	gas2.amount = fuel - gas1.amount;
	into.amount = pressureTempToMols(pressureCap - fuelPressure, intoTemp);
	temperature = mixGasTempsToTemp(fuel, specheat, fuelTemp, into, intoTemp);
	return fuelPressure;
}
float unimixInputSetup(GasType& gas1, GasType& gas2, float temp1, float temp2, float targetTemp) {
	float fuelPressure = (targetTemp / temp2 - 1.0) * pressureCap / (gas1.heatCap / gas2.heatCap - 1.0 + targetTemp * (1.0 / temp2 - gas1.heatCap / gas2.heatCap / temp1));
	gas1.amount = pressureTempToMols(fuelPressure, temp1);
	gas2.amount = pressureTempToMols(pressureCap - fuelPressure, temp2);
	temperature = mixGasTempsToTemp(gas1.amount, gas1.heatCap, temp1, gas2, temp2);
	return fuelPressure;
}
void unimixToInputSetup(GasType& gas1, GasType& gas2, float temp, float secondPerFirst) {
	temperature = temp;
	float total = pressureTempToMols(pressureCap, temperature);
	gas1.amount = total / (1.0 + secondPerFirst);
	gas2.amount = total - gas1.amount;
}

void testOxyplasma() {
	int bestTicks;
	float bestFuelTemp, bestPressure, bestOxygenTemp, maxRadius = 0.0;
	for (float fuelTemp = fireTemp + 2.85; fuelTemp < 600.0; fuelTemp *= temperatureStep) {
		//for (float oxyTemp = fireTemp - 1.0; oxyTemp > 23.0; oxyTemp--) {
			float oxyTemp = 293.15;
			float fuelPressure;
			reset();
			fuelPressure = oxyplasmaInputSetup(fuelTemp, oxyTemp, fireTemp + 1.0);
			loop();
			if (exploded && getCurRange() > maxRadius) {
				maxRadius = getCurRange();
				bestFuelTemp = fuelTemp;
				bestOxygenTemp = oxyTemp;
				bestPressure = fuelPressure;
				bestTicks = tick;
			}
		//}
	}
	cout << "Best oxyplasma: plasma " << bestFuelTemp << "K " << bestPressure << "kPA | oxygen " << bestOxygenTemp <<"K | range " << maxRadius << " ticks " << bestTicks << endl;
}
void testOxyplasmaFuse() {
	int bestTicks;
	float bestFuelTemp, bestPressure, bestOxygenTemp, maxRadius = 0.0;
	for (float fuelTemp = fireTemp + 2.85; fuelTemp < 600.0; fuelTemp *= temperatureStep) {
		//for (float oxyTemp = fireTemp - 1.0; oxyTemp > 23.0; oxyTemp--) {
			float oxyTemp = 293.15;
			float fuelPressure;
			reset();
			fuelPressure = oxyplasmaInputSetup(fuelTemp, oxyTemp, fireTemp + 1.85);
			loop();
			if (exploded && getCurRange() > 8.0 && tick > bestTicks) {
				maxRadius = getCurRange();
				bestFuelTemp = fuelTemp;
				bestOxygenTemp = oxyTemp;
				bestPressure = fuelPressure;
				bestTicks = tick;
			}
		//}
	}
	cout << "Best oxyplasma: plasma " << bestFuelTemp << "K " << bestPressure << "kPa | oxygen " << bestOxygenTemp <<"K | range " << maxRadius << " ticks " << bestTicks << endl;
}
void testOxyplasmaLeakbomb() {
	int maxLeaks;
	float bestFuelTemp, bestPressure, bestOxygenTemp, bestLeakedHeat = 0.0;
	for (float fuelTemp = fireTemp + 9.85; fuelTemp < 600.0; fuelTemp *= temperatureStep) {
		//for (float oxyTemp = fireTemp - 1.0; oxyTemp > 23.0; oxyTemp--) {
			for (float targetTemp = fireTemp + 9.85; targetTemp < fuelTemp; targetTemp *= temperatureStep) {
				float oxyTemp = 293.15;
				float fuelPressure;
				reset();
				fuelPressure = oxyplasmaInputSetup(fuelTemp, oxyTemp, targetTemp);
				loop();
				if (leakedHeat > bestLeakedHeat) {
					maxLeaks = leakCount;
					bestLeakedHeat = leakedHeat;
					bestFuelTemp = fuelTemp;
					bestOxygenTemp = oxyTemp;
					bestPressure = fuelPressure;
				}
			}
		//}
	}
	cout << "Best oxyplasma: plasma " << bestFuelTemp << "K " << bestPressure << "kPa | oxygen " << bestOxygenTemp <<"K | leaks " << maxLeaks << " leaked heat " << bestLeakedHeat << endl;
}
struct bombData {
	float ratio, temp, pressure, thirTemp, radius;
	int ticks;
	float opstat;
};
ostream& operator<<(ostream& out, bombData data) {
	out << "fuel " << 100.0 / (1.0 + data.ratio) << "% first " << data.temp << "K " << data.pressure << "kPa | third " << data.thirTemp << "K | range " << data.radius << " | ticks " << data.ticks << " | optstat " << data.opstat;
	return out;
}
float optimiseStat() {
	return radius;
}
void testTwomix(GasType& gas1, GasType& gas2, GasType& gas3, float mixt1, float mixt2, float thirt1, float thirt2, bool maximise, bool measureBefore) {
	float bestResult = maximise ? 0.0 : numeric_limits<float>::max();
	float bestTemp, bestPressure, bestRatio, bestThirTemp, bestTargetTemp, bestRadius;
	int bestTicks;
	float bestResult_L = maximise ? 0.0 : numeric_limits<float>::max();
	float bestTemp_L, bestPressure_L, bestRatio_L, bestThirTemp_L, bestTargetTemp_L, bestRadius_L;
	int bestTicks_L;
	for (float thirTemp = thirt1; thirTemp <= thirt2; thirTemp *= temperatureStep) {
		for (float fuelTemp = mixt1; fuelTemp <= mixt2; fuelTemp *= temperatureStep) {
			float targetTemp2 = stepTargetTemp ? std::max(thirTemp, fuelTemp) : fireTemp + overTemp + temperatureStep;
			for (float targetTemp = fireTemp + overTemp; targetTemp < targetTemp2; targetTemp *= temperatureStep) {
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
					if (measureBefore) {
						stat = optimiseStat();
					}
					loop();
					if (!measureBefore) {
						stat = optimiseStat();
					}
					if (radius > targetRadius && (maximise == (stat > bestResult))) {
						bestRatio = ratio;
						bestPressure = fuelPressure;
						bestTemp = fuelTemp;
						bestThirTemp = thirTemp;
						bestTargetTemp = targetTemp;
						bestRadius = radius;
						bestTicks = tick;
						bestResult = stat;
					}
					if (logLevel >= 5) {
						cout << "Current: " << bombData{ratio, fuelTemp, fuelPressure, thirTemp, radius, tick, stat} << endl;
					} else if (radius > targetRadius && (maximise == (stat > bestResult_L))) {
						bestRatio_L = ratio;
						bestPressure_L = fuelPressure;
						bestTemp_L = fuelTemp;
						bestThirTemp_L = thirTemp;
						bestTargetTemp_L = targetTemp;
						bestRadius_L = radius;
						bestTicks_L = tick;
						bestResult_L = stat;
					}
				}
				if (logLevel == 4) {
					cout << "Current: " << bombData{bestRatio_L, bestTemp_L, bestPressure_L, bestThirTemp_L, bestRadius_L, bestTicks_L, bestResult_L} << endl;
					bestResult_L = maximise ? 0.0 : numeric_limits<float>::max();
				}
			}
			if (logLevel == 3) {
				cout << "Current: " << bombData{bestRatio_L, bestTemp_L, bestPressure_L, bestThirTemp_L, bestRadius_L, bestTicks_L, bestResult_L} << endl;
				bestResult_L = maximise ? 0.0 : numeric_limits<float>::max();
			}
		}
		if (logLevel == 2) {
			cout << "Current: " << bombData{bestRatio_L, bestTemp_L, bestPressure_L, bestThirTemp_L, bestRadius_L, bestTicks_L, bestResult_L} << endl;
			bestResult_L = maximise ? 0.0 : numeric_limits<float>::max();
		} else if (logLevel == 1) {
			cout << "Best: " << bombData{bestRatio, bestTemp, bestPressure, bestThirTemp, bestRadius, bestTicks, bestResult} << endl;
		}
	}
	cout << "Best: " << bombData{bestRatio, bestTemp, bestPressure, bestThirTemp, bestRadius, bestTicks, bestResult} << endl;
}
void testUnimix(GasType& gas1, GasType& gas2, float firstt1, float firstt2, float sect1, float sect2, bool maximise) {
	float bestResult = maximise ? 0.0 : numeric_limits<float>::max();
	float bestTemp1, bestTemp2, bestRatio, bestRadius;
	int bestTicks;
	for (float temp1 = firstt1; temp1 <= firstt2; temp1 *= temperatureStep) {
		for (float temp2 = sect1; temp2 <= sect2; temp2 *= temperatureStep) {
			float targetTemp2 = stepTargetTemp ? std::max(temp1, temp2) : fireTemp + overTemp + temperatureStep;
			for (float targetTemp = fireTemp + overTemp; targetTemp < targetTemp2; targetTemp *= temperatureStep) {
				reset();
				if (temp1 <= fireTemp && temp2 <= fireTemp) {
					continue;
				}
				if (targetTemp > temp1 == targetTemp > temp2) {
					continue;
				}
				float firstPressure = unimixInputSetup(gas1, gas2, temp1, temp2, targetTemp);
				if (firstPressure > pressureCap || firstPressure < 0.0) {
					continue;
				}
				float ratio = gas2.amount / gas1.amount;
				loop();
				if (radius > targetRadius && (maximise == (optimiseStat() > bestResult))) {
					bestTemp1 = temp1;
					bestTemp2 = temp2;
					bestRatio = ratio;
					bestRadius = radius;
					bestTicks = tick;
					bestResult = optimiseStat();
				}
				if (logLevel == 3) {
					float first = 1.0 / (1.0 + bestRatio);
					cout << "Current: " << first * pressureCap << "kPa/" << (1.0 - first) * pressureCap << "kPa first/second | temp " << bestTemp1 << "K/" << bestTemp2 << "K | radius " << bestRadius << " | ticks " << bestTicks << " | opstat " << bestResult << endl;
				}
			}
			if (logLevel == 2) {
				float first = 1.0 / (1.0 + bestRatio);
				cout << "Current: " << first * pressureCap << "kPa/" << (1.0 - first) * pressureCap << "kPa first/second | temp " << bestTemp1 << "K/" << bestTemp2 << "K | radius " << bestRadius << " | ticks " << bestTicks << " | opstat " << bestResult << endl;
			}
		}
		float first = 1.0 / (1.0 + bestRatio);
		cout << "Current: " << first * pressureCap << "kPa/" << (1.0 - first) * pressureCap << "kPa first/second | temp " << bestTemp1 << "K/" << bestTemp2 << "K | radius " << bestRadius << " | ticks " << bestTicks << " | opstat " << bestResult << endl;
	}
	float first = 1.0 / (1.0 + bestRatio);
	cout << "Best: " << first * pressureCap << "kPa/" << (1.0 - first) * pressureCap << "kPa first/second | temp " << bestTemp1 << "K/" << bestTemp2 << "K | radius " << bestRadius << " | ticks " << bestTicks << " | opstat " << bestResult << endl;
}
void testUnimixToTemp(GasType& gas1, GasType& gas2, float temp, float targetTemp) {
	float closestTemp = std::numeric_limits<float>::min(), bestRatio;
	for (float ratio = 1.0 / ratioFrom; ratio <= ratioTo; ratio *= ratioStep) {
		reset();
		unimixToInputSetup(gas1, gas2, temp, ratio);
		loop();
		if (std::abs(targetTemp - temperature) < std::abs(targetTemp - closestTemp)) {
			closestTemp = temperature;
			bestRatio = ratio;
		}
	}
	cout << "Best: " << 100.0 / (1.0 + bestRatio) << "% first | temp " << closestTemp << "K" << endl;
}
void testUnimixToPressure(GasType& gas1, GasType& gas2, float temp, float targetPressure) {
	float closestPressure = std::numeric_limits<float>::min(), closestTemp, bestRatio;
	for (float ratio = 1.0 / ratioFrom; ratio <= ratioTo; ratio *= ratioStep) {
		reset();
		unimixToInputSetup(gas1, gas2, temp, ratio);
		loop();
		if (std::abs(targetPressure - getPressure()) < std::abs(targetPressure - closestPressure)) {
			closestPressure = getPressure();
			closestTemp = temperature;
			bestRatio = ratio;
		}
	}
	cout << "Best: " << 100.0 / (1.0 + bestRatio) << "% first | temp " << closestTemp << "K | pressure " << closestPressure << "kPa" << endl;
}

void heatCapInputSetup() {
	cout << "Enter `dheat capacities for plasma, oxygen, tritium, water vapor, carbon dioxide: ";
	for (GasType& g : gases) {
		cin >> g.heatCap;
	};
}

int main(int argc, char* argv[]) {
	if (argc > 1) {
		for (int i = 0; i < argc; i++) {
			string arg(argv[i]);
			if (arg[0] != '-' || arg.length() < 2) {
				continue;
			}
			if (arg[1] == '-') {
				if (arg.rfind("--radius", 0) == 0) {
					targetRadius = std::stod(arg.substr(8));
				} else if (arg.rfind("--ticks", 0) == 0) {
					tickCap = std::stoi(arg.substr(7));
				} else if (arg.rfind("--tstep", 0) == 0) {
					temperatureStep = std::stod(arg.substr(7));
				} else if (arg.rfind("--volume", 0) == 0) {
					volume = std::stod(arg.substr(8));
				} else if (arg.rfind("--pressureCap", 0) == 0) {
					pressureCap = std::stod(arg.substr(13));
				} else if (arg.rfind("--overtemp", 0) == 0) {
					overTemp = std::stod(arg.substr(10));
				} else if (arg.rfind("--loglevel", 0) == 0) {
					logLevel = std::stoi(arg.substr(10));
				} else if (arg.rfind("--scrub", 0) == 0) {
					doScrub = !doScrub;
					long long scrubCount = 1;
					cout << "Enter scrubber count: ";
					cin >> scrubCount;
					cout << "Enter volume/scrubber_pump_rate: ";
					cin >> scrubRate;
					scrubRate = pow((1.f - 1.f / scrubRate), scrubCount);
					string gasName;
					do {
						cout << "Enter gas type to scrub (enter invalid gas to stop entry): ";
						cin >> gasName;
						if (isGas(gasName)) {
							scrubGases.push_back(&(stog(gasName)));
						}
					} while (isGas(gasName));
				} else {
					cout << "Unrecognized argument '" << arg << "'." << endl;
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
					cout << "heat capacity ratio (second:first): ";
					cin >> capratio;
					ratio = 100.0 / ratio - 1.0;
					cout << "pressure ratio: " << 100.0 / (1.0 + ratio * t2 / t1) << "% first | temp " << (t1 + t2 * capratio * ratio) / (1.0 + capratio * ratio) << "K";
					return 0;
				}
				case 'o': {
					testOxyplasma();
					return 0;
				}
				case 'f': {
					fullInputSetup();
					loop();
					status();
					return 0;
				}
				case 'u': {
					string gas1, gas2;
					float t11, t12, t21, t22;
					cout << "gas1: ";
					cin >> gas1;
					cout << "gas2: ";
					cin >> gas2;
					cout << "first temp1: ";
					cin >> t11;
					cout << "first temp2: ";
					cin >> t12;
					cout << "second temp1: ";
					cin >> t21;
					cout << "second temp2: ";
					cin >> t22;
					testUnimix(stog(gas1), stog(gas2), t11, t12, t21, t22, true);
					return 0;
				}
				case 't': {
					string gas1, gas2;
					float ti, tt;
					cout << "gas1: ";
					cin >> gas1;
					cout << "gas2: ";
					cin >> gas2;
					cout << "ignition temp: ";
					cin >> ti;
					cout << "target temp: ";
					cin >> tt;
					testUnimixToTemp(stog(gas1), stog(gas2), ti, tt);
					return 0;
				}
				case 'p': {
					string gas1, gas2;
					float ti, tp;
					cout << "gas1: ";
					cin >> gas1;
					cout << "gas2: ";
					cin >> gas2;
					cout << "ignition temp: ";
					cin >> ti;
					cout << "target pressure: ";
					cin >> tp;
					testUnimixToPressure(stog(gas1), stog(gas2), ti, tp);
					return 0;
				}
				case 'h': {
					cout << "Usage: " << argv[0] << " [-h] [-H] [-f] [-r]\n" <<
					"options:\n" <<
					"	-h\n" <<
					"		show help and exit\n" <<
					"	-n\n" <<
					"       assume inside pipe: prevent tank-related effects" <<
					"	-H\n" <<
					"		redefine heat capacities\n" <<
					"	-r\n" <<
					"		set ratio iteration bounds+step\n" <<
					"	-s\n" <<
					"		step mix-to temp (may take a long time)\n" <<
					"	-m\n" <<
					"		different-temperature gas mixing mode\n" <<
					"	-o\n" <<
					"		oxyplasma mode\n" <<
					"	-f\n" <<
					"		try full input\n" <<
					"	-u\n" <<
					"		unimix mode\n" <<
					"	-t\n" <<
					"		unimix-to-temp mode\n" <<
					"	--radius\n" <<
					"		set target radius\n" <<
					"	--ticks\n" <<
					"		set tick limit\n" <<
					"	--tstep\n" <<
					"		set temperature iteration step\n" <<
					"	--volume\n" <<
					"		set tank volume\n" <<
					"	--overtemp\n" <<
					"		delta from the fire temperature to iterate from\n" <<
					"	--loglevel\n" <<
					"		what level of the nested loop to log, 0-5: none, globalBest, thirTemp, fuelTemp, targetTemp, all\n" <<
					"	--scrub\n" <<
					"       lets you configure scrubbing from the gas mix for utility usecases" <<
					"\nss14 maxcap atmos sim\n";
					return 0;
				}
				default: {
					cout << "Unrecognized argument '" << arg << "'." << endl;
					break;
				}
			}
		}
	}
	// testTwomix(oxygen, tritium, plasma, 73.15, 73.15, 374.15, 593.15, &radius, true);
	// testTwomix(oxygen, tritium, frezon, 2.3, 2.3, 2000, 2000, &radius, true);
	cout << "Gases:\n" <<
	"oxygen\n" <<
	"plasma\n" <<
	"tritium\n" <<
	"waterVapour\n" <<
	"carbonDioxide\n" <<
	"frezon\n" <<
	"tip: gas3 is inserted into a mix of gas1 and gas2\n\n";
	string gas1, gas2, gas3;
	float mixt1, mixt2, thirt1, thirt2;
	cout << "gas1: ";
	cin >> gas1;
	cout << "gas2: ";
	cin >> gas2;
	cout << "gas3: ";
	cin >> gas3;
	cout << "mix temp1: ";
	cin >> mixt1;
	cout << "mix temp2: ";
	cin >> mixt2;
	cout << "third temp1: ";
	cin >> thirt1;
	cout << "third temp2: ";
	cin >> thirt2;
	testTwomix(stog(gas1), stog(gas2), stog(gas3), mixt1, mixt2, thirt1, thirt2, true, false);
	return 0;
}

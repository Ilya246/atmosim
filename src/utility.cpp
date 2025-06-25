#include <functional>
#include <iostream>
#include <random>
#include <vector>

#include "utility.hpp"

namespace asim {

float frand() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> distribution;

    return distribution(gen);
}

float frand(float to) {
    return frand() * to;
}

float frand(float from, float to) {
    return from + frand(to - from);
}

std::vector<float> get_fractions(const std::vector<float>& ratios) {
    std::vector<float> fractions(ratios.size());
    float total = std::accumulate(ratios.begin(), ratios.end(), 0.f);
    for (size_t i = 0; i < ratios.size(); ++i) {
        fractions[i] = ratios[i] / total;
    }

    return fractions;
}

void log(std::function<std::string()>&& str, size_t log_level, size_t level, bool endl, bool clear) {
    if (log_level < level) return;
    if (clear) std::cout << "\33[2K\r";
    std::cout << str();
    if (endl) std::cout << std::endl;
}

}

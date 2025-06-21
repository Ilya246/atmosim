#include <random>

inline float frand() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> distribution;

    return distribution(gen);
}

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

std::vector<float>& operator+=(std::vector<float>& lhs, const std::vector<float>& rhs) {
    size_t dims = lhs.size();
    for (size_t i = 0; i < dims; ++i) {
        lhs[i] += rhs[i];
    }
    return lhs;
}

std::vector<float>& operator-=(std::vector<float>& lhs, const std::vector<float>& rhs) {
    size_t dims = lhs.size();
    for (size_t i = 0; i < dims; ++i) {
        lhs[i] -= rhs[i];
    }
    return lhs;
}

std::vector<float> operator-(const std::vector<float>& lhs, const std::vector<float>& rhs) {
    std::vector<float> vec(lhs);
    size_t dims = lhs.size();
    for (size_t i = 0; i < dims; ++i) {
        vec[i] -= rhs[i];
    }
    return vec;
}

std::vector<float>& operator*=(std::vector<float>& lhs, float rhs) {
    size_t dims = lhs.size();
    for (size_t i = 0; i < dims; ++i) {
        lhs[i] *= rhs;
    }
    return lhs;
}

std::vector<float> operator*(const std::vector<float>& lhs, float rhs) {
    size_t dims = lhs.size();
    std::vector<float> vec(lhs);
    for (size_t i = 0; i < dims; ++i) {
        vec[i] *= rhs;
    }
    return vec;
}
std::vector<float> operator*(float lhs, const std::vector<float>& rhs) {
    return rhs * lhs;
}

std::vector<float> lerp(const std::vector<float>& vec, const std::vector<float>& to, float by) {
    std::vector<float> out_vec(vec);
    float self = 1.f - by;
    size_t dims = vec.size();
    for (size_t i = 0; i < dims; ++i) {
        out_vec[i] *= self;
        out_vec[i] += to[i] * by;
    }
    return out_vec;
}

std::vector<float> get_fractions(const std::vector<float>& ratios) {
    std::vector<float> fractions(ratios.size());
    float total = std::accumulate(ratios.begin(), ratios.end(), 0.f);
    for (size_t i = 0; i < ratios.size(); ++i) {
        fractions[i] = ratios[i] / total;
    }
    return fractions;
}

std::vector<float>& normalize(std::vector<float>& vec) {
    float ilen = 1.f / length(vec);
    size_t dims = vec.size();
    for(size_t i = 0; i < dims; ++i) {
        vec[i] *= ilen;
    }
    return vec;
}

std::vector<float>& orthogonalise(std::vector<float>& vec, const std::vector<float>& to) {
    return vec -= to * (dot(vec, to) / dot(to, to));
}

std::vector<float>& lerp_in_place(std::vector<float>& vec, const std::vector<float>& to, float by) {
    size_t dims = vec.size();
    float self = 1.f - by;
    for (size_t i = 0; i < dims; ++i) {
        vec[i] *= self;
        vec[i] += to[i] * by;
    }
    return vec;
}

std::vector<float> orthogonal_noise(const std::vector<float>& dir, float strength) {
    size_t dims = dir.size();
    std::vector<float> noise(dims);
    for(size_t i = 0; i < dims; ++i) {
        noise[i] = frand(-1.f, 1.f);
    }
    orthogonalise(noise, dir);
    return noise *= strength / length(noise);
}

float dot(const std::vector<float>& a, const std::vector<float>& b) {
    float sum = 0.f;
    size_t dims = a.size();;
    for(size_t i = 0; i < dims; ++i) {
        sum += a[i] * b[i];
    }
    return sum;
}

float length(const std::vector<float>& vec) {
    return std::sqrt(dot(vec, vec));
}

void log(std::function<std::string()>&& str, size_t log_level, size_t level, bool endl, bool clear) {
    if (log_level < level) return;
    if (clear) std::cout << "\33[2K\r";
    std::cout << str();
    if (endl) std::cout << std::endl;
}

}

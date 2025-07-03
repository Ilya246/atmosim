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

float round_to(float what, float to) {
    if (to == 0.f) return what;
    return std::round(what / to) * to;
}

long get_float_digits(float num) {
    const long float_digits = std::numeric_limits<float>::digits10;
    long digits = num < std::pow(0.1f, float_digits) ? float_digits : std::max(0l, std::lround(-std::log10(num)));
    return digits;
}

std::string str_round_to(float what, float to) {
    float rounded = round_to(what, to);
    long digits = get_float_digits(to);
    return std::format("{:.{}f}", rounded, digits);
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

std::vector<float> operator+(const std::vector<float>& lhs, const std::vector<float>& rhs) {
    std::vector<float> vec(lhs);
    size_t dims = lhs.size();
    for (size_t i = 0; i < dims; ++i) {
        vec[i] += rhs[i];
    }
    return vec;
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

std::vector<float> lerp(std::vector<float> vec, const std::vector<float>& to, float by) {
    float self = 1.f - by;
    size_t dims = vec.size();
    for (size_t i = 0; i < dims; ++i) {
        vec[i] *= self;
        vec[i] += to[i] * by;
    }
    return vec;
}

std::vector<float> get_fractions(std::vector<float> ratios) {
    float i_total = 1.f / std::accumulate(ratios.begin(), ratios.end(), 0.f);
    for (size_t i = 0; i < ratios.size(); ++i) {
        ratios[i] *= i_total;
    }
    return ratios;
}

std::vector<float>& normalize(std::vector<float>& vec) {
    float ilen = 1.f / length(vec);
    size_t dims = vec.size();
    for(size_t i = 0; i < dims; ++i) {
        vec[i] *= ilen;
    }
    return vec;
}

std::vector<float>& vec_zero_if(std::vector<float>& vec, const std::vector<bool>& if_vec) {
    size_t dims = vec.size();
    for (size_t i = 0; i < dims; ++i) {
        vec[i] *= if_vec[i] ? 0.f : 1.f;
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

std::vector<float> normalized(const std::vector<float>& vec) {
    return vec * (1.f / length(vec));
}

std::vector<float> random_vec(size_t dims, float scale) {
    std::vector<float> out_vec(dims);
    for (float& f : out_vec) f = frand(-scale, scale);
    return out_vec;
}

std::vector<float> random_vec(size_t dims, float scale, float len) {
    return normalized(random_vec(dims, scale)) * len;
}

std::vector<float> random_vec(const std::vector<float>& lower_bounds, const std::vector<float>& upper_bounds) {
    size_t dims = lower_bounds.size();
    std::vector<float> out_vec(dims);
    for (size_t i = 0; i < dims; ++i) {
        out_vec[i] = frand(lower_bounds[i], upper_bounds[i]);
    }
    return out_vec;
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

bool vec_in_bounds(const std::vector<float>& vec, const std::vector<float>& lower, const std::vector<float>& upper) {
    size_t dims = vec.size();
    for (size_t i = 0; i < dims; ++i) {
        if (vec[i] < lower[i] || vec[i] > upper[i]) return false;
    }
    return true;
}

void space_vectors(std::vector<std::vector<float>>& vecs, float strength) {
    size_t vec_n = vecs.size();
    size_t dims = vecs[0].size();
    std::vector<float> lengths(vec_n);
    for (size_t i = 0; i < vec_n; ++i) lengths[i] = length(vecs[i]);
    std::vector<std::vector<float>> adj_by(vecs.size());
    for (size_t a_idx = 0; a_idx < vec_n; ++a_idx) {
        std::vector<float>& vec_a = vecs[a_idx];
        std::vector<float>& adj_vec = adj_by[a_idx];
        adj_vec = std::vector<float>(dims, 0.f);
        for (size_t b_idx = 0; b_idx < vec_n; ++b_idx) {
            const std::vector<float>& vec_b = vecs[b_idx];
            if (vec_a == vec_b) continue;
            std::vector<float> diff = vec_a - vec_b;
            adj_vec += diff * (strength / dot(diff, diff));
        }
    }
    for (size_t a_idx = 0; a_idx < vec_n; ++a_idx) {
        std::vector<float>& vec_a = vecs[a_idx];
        vec_a += adj_by[a_idx];
        vec_a *= lengths[a_idx] / length(vec_a); // preserve length
    }
}

void log(std::function<std::string()>&& str, size_t log_level, size_t level, bool endl, bool clear) {
    if (log_level < level) return;
    log_mutex.lock();
    if (clear) std::cout << "\33[2K\r";
    std::cout << str();
    if (endl) std::cout << std::endl;
    log_mutex.unlock();
}

duration_t as_seconds(float count) {
    return std::chrono::duration_cast<duration_t>(std::chrono::duration<float>(count));
}

float to_seconds(duration_t duration) {
    return std::chrono::duration_cast<std::chrono::duration<float>>(duration).count();
}

}

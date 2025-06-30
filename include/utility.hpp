#pragma once

#include <argparse/read.hpp>

#include <csignal>
#include <format>
#include <functional>
#include <mutex>
#include <numeric>
#include <string>
#include <vector>

#include "constants.hpp"

// define this to omit exception checks in hotcode
#ifdef ASIM_NOEXCEPT
#define CHECKEXCEPT if constexpr (false)
#else
#define CHECKEXCEPT if constexpr (true)
#endif

namespace asim {

float frand();
float frand(float to);
float frand(float from, float to);

float round_to(float what, float to);

// vec-vec operators
std::vector<float>& operator+=(std::vector<float>& lhs, const std::vector<float>& rhs);
std::vector<float>& operator-=(std::vector<float>& lhs, const std::vector<float>& rhs);
std::vector<float> operator+(const std::vector<float>& lhs, const std::vector<float>& rhs);
std::vector<float> operator-(const std::vector<float>& lhs, const std::vector<float>& rhs);

// vec-num operators
std::vector<float>& operator*=(std::vector<float>& lhs, float rhs);
std::vector<float> operator*(const std::vector<float>& lhs, float rhs);
std::vector<float> operator*(float lhs, const std::vector<float>& rhs);

// ->vector non-modifying operations
std::vector<float> lerp(std::vector<float> vec, const std::vector<float>& to, float by);
std::vector<float> get_fractions(std::vector<float> ratios);

// modifying operations
std::vector<float>& normalize(std::vector<float>& vec);
std::vector<float>& vec_zero_if(std::vector<float>& vec, const std::vector<bool>& if_vec);
std::vector<float>& orthogonalise(std::vector<float>& vec, const std::vector<float>& to);
std::vector<float>& lerp_in_place(std::vector<float>& vec, const std::vector<float>& to, float by);

// ->vec non-modifying operations
std::vector<float> normalized(const std::vector<float>& vec);
std::vector<float> random_vec(size_t dims, float scale);
std::vector<float> random_vec(size_t dims, float scale, float len);
std::vector<float> random_vec(const std::vector<float>& lower_bounds, const std::vector<float>& upper_bounds);
std::vector<float> orthogonal_noise(const std::vector<float>& dir, float strength);

// ->num non-modifying operations
float length(const std::vector<float>& vec);
float dot(const std::vector<float>& a, const std::vector<float>& b);

bool vec_in_bounds(const std::vector<float>& vec, const std::vector<float>& lower, const std::vector<float>& upper);

// tries to rotate input vectors to be spaced apart, expensive
void space_vectors(std::vector<std::vector<float>>& vecs, float strength);

inline std::mutex log_mutex;
void log(std::function<std::string()>&& str, size_t log_level, size_t level, bool endl = true, bool clear = true);

duration_t as_seconds(float count);

template<typename L, typename R>
inline std::istream& operator>>(std::istream& lhs, std::pair<L, R>& rhs) {
    std::string str;
    lhs >> str;
    std::tuple<L, R> tup = argp::parse_value<std::tuple<L, R>>(str);
    rhs = {std::get<0>(tup), std::get<1>(tup)};
    return lhs;
}

template<typename T>
std::vector<std::pair<T, float>> get_fractions(const std::vector<std::pair<T, float>>& ratios) {
    std::vector<std::pair<T, float>> fractions(ratios.size());
    float total = std::accumulate(ratios.begin(), ratios.end(), 0.f, [](float lhs, const auto& rhs){ return lhs + rhs.second; });
    for (size_t i = 0; i < ratios.size(); ++i) {
        fractions[i] = {ratios[i].first, ratios[i].second / total};
    }

    return fractions;
}

template<typename T>
inline std::string vec_to_str(const std::vector<T>& vec, std::string_view sep = ", ", std::function<std::string(const T&)>&& fmt = [](const T& arg){ return std::format("{}", arg); }) {
    size_t to = vec.size();
    if (to == 0) return "[empty]";
    std::string out_str = std::format("{}", fmt(vec[0]));
    for (size_t i = 1; i < to; ++i) {
        out_str += std::format("{}{}", sep, fmt(vec[i]));
    }
    return out_str;
}

template<typename V>
inline std::string vec_to_str(const std::vector<std::vector<V>>& vec, std::string_view sep_inner = ", ", std::string_view sep_outer = ", ") {
    return vec_to_str<std::vector<V>>(vec, sep_outer, [&](const std::vector<V>& arg){ return std::format("[{}]", vec_to_str(arg, sep_inner)); });
}

inline volatile sig_atomic_t status_SIGINT = 0;

inline void sigint_hander(int signum) {
    if (signum == SIGINT) {
        status_SIGINT = 1;
    }
}

inline void handle_sigint() {
#ifdef _WIN32
    // TODO: make it work on windows
#else
    struct sigaction sa;
    sa.sa_handler = sigint_hander;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
#endif
}

}

template<typename L, typename R>
struct argp::is_container<std::pair<L, R>> : std::true_type {};

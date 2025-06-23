#pragma once

#include <format>
#include <functional>
#include <string>
#include <vector>

// define this to omit exception checks in hotcode
#ifdef ASIM_NOEXCEPT
#define CHECKEXCEPT if constexpr (false)
#else
#define CHECKEXCEPT if constexpr (true)
#endif

namespace asim {

float frand();

std::vector<float> get_fractions(const std::vector<float>& ratios);

void log(std::function<std::string()>&& str, size_t log_level, size_t level, bool endl = true, bool clear = true);

template<typename T>
inline std::string vec_to_str(const std::vector<T>& vec) {
    size_t to = vec.size();
    if (to == 0) return "[empty]";
    std::string out_str = std::format("{}", vec[0]);
    for (size_t i = 1; i < to; ++i) {
        out_str += std::format(", {}", vec[i]);
    }
    return out_str;
}


}

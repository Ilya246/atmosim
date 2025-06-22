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

}

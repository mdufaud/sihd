#ifndef __SIHD_UTIL_NUM_HPP__
#define __SIHD_UTIL_NUM_HPP__

#include <sys/types.h>

#include <cstdint>
#include <limits>

namespace sihd::util::num
{

uint64_t rand(uint64_t from = 0, uint64_t to = std::numeric_limits<uint64_t>::max());
float frand(float from = 0.0, float to = 1.0);
double drand(double from = 0.0, double to = 1.0);

size_t size(uint64_t number, uint16_t base);

template <typename T>
bool near(const T & value, const T & expected, const T & abs_error)
{
    return std::abs(value - expected) <= std::abs(abs_error);
}

} // namespace sihd::util::num

#endif

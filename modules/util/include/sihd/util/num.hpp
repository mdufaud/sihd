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

template <typename T>
T add_no_overflow(T a, T b)
{
    if constexpr (std::is_unsigned_v<T>)
    {
        if (b > std::numeric_limits<T>::max() - a)
            return std::numeric_limits<T>::max();
        return a + b;
    }
    else
    {
        if ((b > 0) && (a > std::numeric_limits<T>::max() - b))
            return std::numeric_limits<T>::max();
        if ((b < 0) && (a < std::numeric_limits<T>::min() - b))
            return std::numeric_limits<T>::min();
        return a + b;
    }
}

template <typename T>
T substract_no_overflow(T a, T b)
{
    if constexpr (std::is_unsigned_v<T>)
    {
        if (b > a)
            return 0;
        return a - b;
    }
    else
    {
        if ((b > 0) && (a < std::numeric_limits<T>::min() + b))
            return std::numeric_limits<T>::min();
        if ((b < 0) && (a > std::numeric_limits<T>::max() + b))
            return std::numeric_limits<T>::max();
        return a - b;
    }
}

} // namespace sihd::util::num

#endif

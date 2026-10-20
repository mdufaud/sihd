#ifndef __SIHD_UTIL_RANDOM_HPP__
#define __SIHD_UTIL_RANDOM_HPP__

#include <cstdint>
#include <limits>
#include <random>

namespace sihd::util
{

// one seed gives one sequence per standard library: libstdc++, libc++ and msvc distributions differ
class Random
{
    public:
        explicit Random(uint64_t seed);
        ~Random() = default;

        uint64_t rand(uint64_t from = 0, uint64_t to = std::numeric_limits<uint64_t>::max());
        float frand(float from = 0.0, float to = 1.0);
        double drand(double from = 0.0, double to = 1.0);

    private:
        std::mt19937_64 _engine;
};

} // namespace sihd::util

#endif

#include <utility>

#include <sihd/util/Random.hpp>

namespace sihd::util
{

Random::Random(uint64_t seed): _engine(seed) {}

uint64_t Random::rand(uint64_t from, uint64_t to)
{
    if (from > to)
        std::swap(from, to);
    return std::uniform_int_distribution<uint64_t>(from, to)(_engine);
}

float Random::frand(float from, float to)
{
    if (from > to)
        std::swap(from, to);
    return std::uniform_real_distribution<float>(from, to)(_engine);
}

double Random::drand(double from, double to)
{
    if (from > to)
        std::swap(from, to);
    return std::uniform_real_distribution<double>(from, to)(_engine);
}

} // namespace sihd::util

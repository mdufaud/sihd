#include <random>
#include <sihd/util/num.hpp>

namespace sihd::util::num
{

uint64_t rand(uint64_t from, uint64_t to)
{
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<uint64_t> dist(from, to);
    return dist(rng);
}

float frand(float from, float to)
{
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_real_distribution<float> dist(from, to);
    return dist(rng);
}

double drand(double from, double to)
{
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_real_distribution<double> dist(from, to);
    return dist(rng);
}

size_t size(uint64_t number, uint16_t base)
{
    size_t ret = 0;
    while (number != 0)
    {
        ++ret;
        number /= base;
    }
    return ret == 0 ? 1 : ret;
}

} // namespace sihd::util::num
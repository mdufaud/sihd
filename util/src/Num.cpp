#include <sihd/util/Num.hpp>
#include <random>

namespace sihd::util
{

uint64_t    Num::rand(uint64_t from, uint64_t to)
{
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<uint64_t> dist(from, to);
    return dist(rng);
}

float    Num::frand(float from, float to)
{
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_real_distribution<float> dist(from, to);
    return dist(rng);
}

double    Num::drand(double from, double to)
{
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_real_distribution<double> dist(from, to);
    return dist(rng);
}

size_t  Num::size(uint64_t number, uint16_t base)
{
    size_t ret = 0;
    while (number != 0)
    {
        ++ret;
        number /= base;
    }
    return ret == 0 ? 1 : ret;
}

}
#ifndef __SIHD_UTIL_NUM_HPP__
# define __SIHD_UTIL_NUM_HPP__

# include <stdint.h>
# include <sys/types.h>

# include <limits>

namespace sihd::util
{

class Num
{
    public:
        static size_t size(uint64_t number, uint16_t base);

        static uint64_t rand(uint64_t from = 0, uint64_t to = std::numeric_limits<uint64_t>::max());
        static float frand(float from = 0.0, float to = 1.0);
        static double drand(double from = 0.0, double to = 1.0);

    protected:

    private:
        Num() {};
        virtual ~Num() {};
};

}

#endif
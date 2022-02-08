#include <sihd/util/Num.hpp>

namespace sihd::util
{

size_t  Num::get_size(int64_t number, uint16_t base)
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
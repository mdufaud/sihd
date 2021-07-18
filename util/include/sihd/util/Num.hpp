#ifndef __SIHD_UTIL_NUM_HPP__
# define __SIHD_UTIL_NUM_HPP__

# include <stdint.h>
# include <sys/types.h>

namespace sihd::util
{

class Num
{
    public:
        static size_t  get_size(ssize_t number, uint16_t base);

    protected:
    
    private:
        Num() {};
        virtual ~Num() {};
};

}

#endif 
#ifndef __SIHD_UTIL_ISTEPPABLE_HPP__
# define __SIHD_UTIL_ISTEPPABLE_HPP__

namespace sihd::util
{

class ISteppable
{
    public:
        virtual ~ISteppable() {};
        virtual bool    step(std::time_t delta = 0) = 0;
};

}

#endif 
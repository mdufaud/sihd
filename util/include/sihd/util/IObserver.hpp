#ifndef __SIHD_UTIL_OBSERVER_HPP__
# define __SIHD_UTIL_OBSERVER_HPP__

namespace sihd::util
{

template <typename T>
class IObserver
{
    public:
        virtual ~IObserver() {};
        virtual void    observable_changed(T *obs) = 0;
};

}

#endif 
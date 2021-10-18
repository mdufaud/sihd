#ifndef __SIHD_UTIL_IPROVIDER_HPP__
# define __SIHD_UTIL_IPROVIDER_HPP__

namespace sihd::util
{

template <typename ...T>
class IProvider
{
    public:
        virtual ~IProvider() {};
		virtual bool provide(T... args) = 0;
};

}

#endif
#ifndef __SIHD_UTIL_IPROVIDER_HPP__
#define __SIHD_UTIL_IPROVIDER_HPP__

namespace sihd::util
{

template <typename T>
class IProvider
{
    public:
        virtual ~IProvider() {};
        // provides a data
        virtual bool provide(T *data) = 0;
        // checks if the provider is still providing
        virtual bool providing() const = 0;
};

} // namespace sihd::util

#endif
#ifndef __SIHD_UTIL_IPROVIDER_HPP__
#define __SIHD_UTIL_IPROVIDER_HPP__

namespace sihd::util
{

template <typename T>
class IProvider
{
    public:
        virtual ~IProvider() = default;
        ;
        virtual bool provide(T *data) = 0;
        virtual bool providing() const = 0;
};

} // namespace sihd::util

#endif
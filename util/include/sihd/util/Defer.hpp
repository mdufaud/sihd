#ifndef __SIHD_UTIL_DEFER_HPP__
#define __SIHD_UTIL_DEFER_HPP__

#include <functional>

namespace sihd::util
{

class Defer
{
    public:
        using Callback = std::function<void()>;

        Defer(Callback && cb): _cb(std::move(cb)) {}

        ~Defer() { _cb(); }

    private:
        Callback _cb;
};

} // namespace sihd::util

#endif
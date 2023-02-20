#ifndef __SIHD_UTIL_ISTOPPABLERUNNABLE_HPP__
#define __SIHD_UTIL_ISTOPPABLERUNNABLE_HPP__

#include <sihd/util/IRunnable.hpp>

namespace sihd::util
{

class IStoppableRunnable: public IRunnable
{
    public:
        virtual ~IStoppableRunnable() {};
        virtual bool stop() = 0;
        virtual bool is_running() const = 0;
};

} // namespace sihd::util

#endif
#ifndef __SIHD_UTIL_RUNNABLE_HPP__
#define __SIHD_UTIL_RUNNABLE_HPP__

#include <functional>

#include <sihd/util/IRunnable.hpp>

namespace sihd::util
{

class Runnable: public IRunnable
{
    public:
        Runnable();
        Runnable(IRunnable *runnable_ptr);
        Runnable(std::function<bool()> && fun);
        ~Runnable();

        bool operator()() { return this->run(); };

        void set_runnable(IRunnable *runnable_ptr);
        void set_method(std::function<bool()> fun);

        template <typename C>
        void set_method(C *obj, bool (C::*fun)())
        {
            _run_fun = [obj, fun]() -> bool {
                return (obj->*fun)();
            };
        }

        bool has_method() const;
        bool run();

    protected:

    private:
        std::function<bool()> _run_fun;
};

} // namespace sihd::util

#endif
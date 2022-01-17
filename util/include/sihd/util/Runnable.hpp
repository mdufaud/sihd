#ifndef __SIHD_UTIL_RUNNABLE_HPP__
# define __SIHD_UTIL_RUNNABLE_HPP__

# include <sihd/util/IRunnable.hpp>
# include <functional>

namespace sihd::util
{

class Runnable: public IRunnable
{
    public:
        Runnable();
        Runnable(std::function<bool()> fun);
        virtual ~Runnable();

        void set_method(std::function<bool()> fun);

        template <typename C>
        void set_method(C* obj, bool (C::*fun)())
        {
            _run_fun = [obj, fun] () -> bool
            {
                return (obj->*fun)();
            };
        }

        bool has_method();
        bool run();

    protected:

    private:
        std::function<bool()> _run_fun;
};

}

#endif
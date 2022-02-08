#include <sihd/util/Runnable.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::util
{

SIHD_LOGGER;

Runnable::Runnable()
{
}

Runnable::Runnable(std::function<bool()> fun)
{
    _run_fun = std::move(fun);
}

Runnable::~Runnable()
{
}

void    Runnable::set_method(std::function<bool()> fun)
{
    _run_fun = std::move(fun);
}

bool    Runnable::run()
{
    if (_run_fun)
        return _run_fun();
    return false;
}

bool    Runnable::has_method()
{
    return _run_fun ? true : false;
}


}
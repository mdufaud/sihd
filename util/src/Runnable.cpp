#include <sihd/util/Logger.hpp>
#include <sihd/util/Runnable.hpp>

namespace sihd::util
{

SIHD_LOGGER;

Runnable::Runnable() {}

Runnable::Runnable(IRunnable *runnable_ptr)
{
    this->set_runnable(runnable_ptr);
}

Runnable::Runnable(std::function<bool()> && fun)
{
    _run_fun = std::move(fun);
}

Runnable::~Runnable() {}

void Runnable::set_runnable(IRunnable *runnable_ptr)
{
    _run_fun = [runnable_ptr]() -> bool {
        return runnable_ptr->run();
    };
}

void Runnable::set_method(std::function<bool()> fun)
{
    _run_fun = std::move(fun);
}

bool Runnable::run()
{
    if (_run_fun)
        return _run_fun();
    return false;
}

bool Runnable::has_method() const
{
    return _run_fun ? true : false;
}

} // namespace sihd::util
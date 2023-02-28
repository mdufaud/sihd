#include <sihd/util/Timer.hpp>

namespace sihd::util
{

Timer::Timer(const Callback & callback, Timestamp timeout): _stop(false)
{
    _thread = std::thread([this, callback, timeout] {
        _waitable.wait_for(timeout);
        if (_stop == false)
            callback();
    });
}

void Timer::cancel()
{
    _stop = true;
    _waitable.notify_all();
    if (_thread.joinable())
    {
        _thread.join();
    }
}

Timer::~Timer()
{
    this->cancel();
}

} // namespace sihd::util

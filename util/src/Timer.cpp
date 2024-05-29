#include <sihd/util/Timer.hpp>

namespace sihd::util
{

Timer::Timer(Callback && callback, Timestamp timeout): _stop(false)
{
    _thread = std::thread([this, callback, timeout] {
        _waitable.wait_for(timeout, [this] { return _stop == true; });
        if (_stop == false)
            callback();
    });
}

Timer::~Timer()
{
    this->cancel();
}

void Timer::cancel()
{
    {
        auto l = _waitable.guard();
        _stop = true;
        _waitable.notify();
    }
    if (_thread.joinable())
        _thread.join();
}

} // namespace sihd::util

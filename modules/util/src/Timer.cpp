#include <sihd/util/Logger.hpp>
#include <sihd/util/Timer.hpp>

namespace sihd::util
{

SIHD_LOGGER;

Timer::Timer(Callback && callback, Duration timeout): _stop(false)
{
    _thread = std::thread([this, callback, timeout] {
        _waitable.wait_for(timeout, [this] { return _stop == true; });
        if (_stop == false)
        {
            try
            {
                callback();
            }
            catch (const std::exception & e)
            {
                SIHD_LOG(error, "Timer: callback threw: {}", e.what());
            }
            catch (...)
            {
                SIHD_LOG(error, "Timer: callback threw a non standard exception");
            }
        }
    });
}

Timer::~Timer()
{
    this->cancel();
}

void Timer::cancel()
{
    {
        auto lock = _waitable.guard();
        _stop = true;
        _waitable.notify();
    }
    if (_thread.joinable())
        _thread.join();
}

} // namespace sihd::util

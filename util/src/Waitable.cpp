#include <sihd/util/Logger.hpp>
#include <sihd/util/Waitable.hpp>

namespace sihd::util
{

SIHD_LOGGER;

using namespace std::chrono;

Waitable::Waitable() {}

Waitable::~Waitable()
{
    // notifies all
    this->cancel_loop();
}

void Waitable::cancel_loop()
{
    this->notify_all();
}

void Waitable::notify_all()
{
    _condition.notify_all();
}

void Waitable::notify(int times)
{
    for (int i = 0; i < times; ++i)
    {
        _condition.notify_one();
    }
}

void Waitable::wait()
{
    std::unique_lock lock(_mutex);
    _condition.wait(lock);
}

bool Waitable::wait_until(Timestamp timestamp)
{
    std::unique_lock lock(_mutex);
    return _condition.wait_until(lock, std::chrono::system_clock::time_point(std::chrono::nanoseconds(timestamp)))
           == std::cv_status::timeout;
}

bool Waitable::wait_for(Timestamp duration)
{
    std::unique_lock lock(_mutex);
    return _condition.wait_for(lock, std::chrono::nanoseconds(duration)) == std::cv_status::timeout;
}

Timestamp Waitable::wait_elapsed()
{
    Stopwatch hg;
    this->wait();
    return hg.time();
}

Timestamp Waitable::wait_until_elapsed(Timestamp timestamp)
{
    Stopwatch hg;
    this->wait_until(timestamp);
    return hg.time();
}

Timestamp Waitable::wait_for_elapsed(Timestamp duration)
{
    Stopwatch hg;
    this->wait_for(duration);
    return hg.time();
}

std::mutex & Waitable::mutex()
{
    return _mutex;
}

std::lock_guard<std::mutex> Waitable::guard()
{
    return std::lock_guard(_mutex);
}

} // namespace sihd::util

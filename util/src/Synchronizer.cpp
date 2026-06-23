#include <stdexcept>

#include <sihd/util/Logger.hpp>
#include <sihd/util/Synchronizer.hpp>
#include <sihd/util/Waitable.hpp>

namespace sihd::util
{

Synchronizer::Synchronizer(int32_t total): _sync_count(0), _wanted_count(0), _total_sync_round(0)
{
    this->init_sync(total);
}

Synchronizer::Synchronizer(): _sync_count(0), _wanted_count(0), _total_sync_round(0) {}

Synchronizer::~Synchronizer()
{
    this->reset();
}

bool Synchronizer::init_sync(int32_t total)
{
    if (total <= 0)
        throw std::runtime_error("Cannot initialize Synchroniser with a negative or zero value");
    int32_t expected = 0;
    return _wanted_count.compare_exchange_strong(expected, total);
}

void Synchronizer::reset()
{
    _total_sync_round.fetch_add(1);
    _sync_count = 0;
    _waitable.notify_all();
    _wanted_count = 0;
}

void Synchronizer::sync()
{
    (void)this->sync(Duration {});
}

bool Synchronizer::sync(Duration timeout)
{
    const int32_t current_sync_round = _total_sync_round.load();
    if (_sync_count.fetch_add(1) + 1 >= _wanted_count)
    {
        // the last thread to sync - reset count and wake everyone
        {
            auto l = _waitable.guard();
            _sync_count = 0;
            // new round of sync unlocks
            _total_sync_round.fetch_add(1);
        }
        _waitable.notify_all();
        return true;
    }
    // wait for the current round to finish
    const auto pred = [this, current_sync_round] {
        return _total_sync_round.load() != current_sync_round;
    };
    if (timeout.nanoseconds() <= 0)
    {
        _waitable.wait(pred);
        return true;
    }
    // false for timeout
    return _waitable.wait_for(timeout, pred);
}

} // namespace sihd::util
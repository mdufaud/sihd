#include <unistd.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/Synchronizer.hpp>
#include <sihd/util/Waitable.hpp>

namespace sihd::util
{

Synchronizer::Synchronizer(): _sync_count(0), _wanted_count(0) {}

Synchronizer::~Synchronizer()
{
    this->reset();
}

void Synchronizer::init_sync(int32_t total)
{
    _wanted_count = std::max(0, total);
}

void Synchronizer::reset()
{
    while (_sync_count.load() > 0)
        _waitable.notify_all();
    this->init_sync(0);
}

void Synchronizer::sync()
{
    if (_sync_count.fetch_add(1) + 1 >= _wanted_count)
    {
        // the last thread to sync
        --_sync_count;
        while (_sync_count.load() > 0)
            _waitable.notify_all();
    }
    else
    {
        _waitable.wait();
        --_sync_count;
    }
}

} // namespace sihd::util
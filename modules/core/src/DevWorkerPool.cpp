#include <algorithm>
#include <thread>

#include <sihd/core/DevWorkerPool.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

namespace sihd::core
{

using namespace sihd::util;

SIHD_REGISTER_FACTORY(DevWorkerPool)

SIHD_LOGGER;

DevWorkerPool::DevWorkerPool(const std::string & name, Node *parent):
    DevWorker(name, parent),
    _threads(0),
    _in_flight(0)
{
    this->set_threads(std::max(std::thread::hardware_concurrency(), 1u));
    this->add_conf("threads", &DevWorkerPool::set_threads);
}

DevWorkerPool::~DevWorkerPool()
{
    if (this->is_running())
        this->stop();
}

bool DevWorkerPool::is_running() const
{
    return _pool.has_value();
}

bool DevWorkerPool::set_threads(size_t threads)
{
    if (threads == 0)
        return false;
    _threads = threads;
    return true;
}

void DevWorkerPool::handle(sihd::core::Channel *c)
{
    Channel *out = this->relay_out(c);
    if (out == nullptr)
        return;
    Lane & lane = _lanes.at(out);

    ArrByte data;
    data.resize(c->array()->byte_size());
    if (c->copy_to(data) == false)
        return;

    if (lane.pending.push(std::move(data), this->queue_size()) == false)
    {
        this->count_drop();
        return;
    }
    if (lane.unwritten.fetch_add(1) == 0)
    {
        {
            auto guard = _idle.guard();
            ++_in_flight;
        }
        this->_submit(out, lane);
    }
}

void DevWorkerPool::_submit(Channel *out, Lane & lane)
{
    _pool->add_job([this, out, &lane] { this->_relay_one(out, lane); });
}

void DevWorkerPool::_relay_one(Channel *out, Lane & lane)
{
    // a push always lands before the count that schedules or keeps this job
    std::optional<ArrByte> data = lane.pending.try_pop();
    if (out->write(*data) == false)
        SIHD_LOG(warning, "DevWorkerPool: {} could not write {}", this->name(), out->name());
    this->publish_dropped();

    if (lane.unwritten.fetch_sub(1) > 1)
    {
        this->_submit(out, lane);
        return;
    }
    {
        auto guard = _idle.guard();
        --_in_flight;
    }
    _idle.notify_all();
}

bool DevWorkerPool::on_start()
{
    if (!this->bind_relays())
        return false;
    _lanes.clear();
    for (const auto & [in, out] : this->relays())
        _lanes.try_emplace(out);
    _pool.emplace(this->name(), _threads);
    this->observe_relays();
    return true;
}

bool DevWorkerPool::on_stop()
{
    this->remove_channels_observation();
    _idle.wait([this] { return _in_flight == 0; });
    _pool.reset();
    return true;
}

} // namespace sihd::core

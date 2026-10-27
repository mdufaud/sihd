#include <sihd/core/DevStats.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/Value.hpp>

namespace sihd::core
{

using namespace sihd::util;

SIHD_REGISTER_FACTORY(DevStats)

SIHD_LOGGER;

namespace
{

bool is_numeric(Type type)
{
    return type != TYPE_NONE && type != TYPE_BOOL && type != TYPE_CHAR && type != TYPE_BYTE && type != TYPE_UBYTE
           && type != TYPE_OBJECT;
}

} // namespace

DevStats::DevStats(const std::string & name, Node *parent):
    sihd::core::Device(name, parent),
    _step_worker(this),
    _start_time(0)
{
    constexpr int default_period_ms = 1000;
    this->set_period(default_period_ms);
    this->add_conf("period", &DevStats::set_period);
}

DevStats::~DevStats()
{
    _step_worker.stop_worker();
}

bool DevStats::is_running() const
{
    return _step_worker.is_worker_running();
}

bool DevStats::set_period(int milliseconds)
{
    if (milliseconds <= 0)
        return false;
    return _step_worker.set_frequency(1000.0 / milliseconds);
}

void DevStats::handle(sihd::core::Channel *c)
{
    Sample sample {c, c->array()->byte_size(), {}};
    if (is_numeric(c->array()->data_type()))
    {
        const size_t samples = c->array()->size();
        sample.values.reserve(samples);
        for (size_t idx = 0; idx < samples; ++idx)
            sample.values.push_back(c->value_at(idx).to_double());
    }
    _pending.push(std::move(sample));
}

void DevStats::_accumulate()
{
    while (std::optional<Sample> sample = _pending.try_pop())
    {
        const auto it = _lanes.find(sample->in);
        if (it == _lanes.end())
            continue;
        Lane & lane = it->second;
        ++lane.count;
        lane.bytes += sample->bytes;
        for (const double value : sample->values)
            lane.values.add_sample(value);
    }
}

bool DevStats::run()
{
    this->_accumulate();
    const double elapsed = static_cast<double>((_clock.now() - _start_time).nanoseconds()) / 1e9;
    for (const auto & [in, lane] : _lanes)
    {
        const double rate = elapsed > 0.0 ? static_cast<double>(lane.count) / elapsed : 0.0;
        const double byte_rate = elapsed > 0.0 ? static_cast<double>(lane.bytes) / elapsed : 0.0;

        // count goes last because an observer of count reads a snapshot where every other channel is up to date
        lane.bytes_ptr->write<uint64_t>(0, lane.bytes);
        lane.byte_rate_ptr->write<double>(0, byte_rate);
        lane.rate_ptr->write<double>(0, rate);
        lane.min_ptr->write<double>(0, lane.values.min);
        lane.max_ptr->write<double>(0, lane.values.max);
        lane.mean_ptr->write<double>(0, lane.values.average());
        lane.count_ptr->write<uint64_t>(0, lane.count);
    }
    return true;
}

Channel *DevStats::_output(const std::string & name, Type type)
{
    Channel *channel = this->get_channel(name).value_or(nullptr);
    return channel != nullptr ? channel : this->add_channel(name, type, 1);
}

bool DevStats::_bind_lane(const std::string & name, Lane & lane)
{
    lane.count_ptr = this->_output(name + "_count", TYPE_ULONG);
    lane.rate_ptr = this->_output(name + "_rate", TYPE_DOUBLE);
    lane.bytes_ptr = this->_output(name + "_bytes", TYPE_ULONG);
    lane.byte_rate_ptr = this->_output(name + "_byte_rate", TYPE_DOUBLE);
    lane.min_ptr = this->_output(name + "_min", TYPE_DOUBLE);
    lane.max_ptr = this->_output(name + "_max", TYPE_DOUBLE);
    lane.mean_ptr = this->_output(name + "_mean", TYPE_DOUBLE);
    return lane.count_ptr != nullptr && lane.rate_ptr != nullptr && lane.bytes_ptr != nullptr
           && lane.byte_rate_ptr != nullptr && lane.min_ptr != nullptr && lane.max_ptr != nullptr
           && lane.mean_ptr != nullptr;
}

bool DevStats::on_start()
{
    auto inputs = this->linked_channels();
    if (SIHD_UNEXPECTED_LOG(inputs))
        return false;

    _lanes.clear();
    _pending.clear();
    for (const auto & [name, in] : *inputs)
    {
        if (this->_bind_lane(name, _lanes[in]) == false)
            return false;
    }
    _start_time = _clock.now();
    this->run();
    if (_step_worker.start_worker(this->name()) == false)
    {
        SIHD_LOG(error, "DevStats: {} could not start its thread", this->name());
        return false;
    }
    for (const auto & [name, in] : *inputs)
        this->observe_channel(in);
    return true;
}

bool DevStats::on_stop()
{
    this->remove_channels_observation();
    const bool stopped = _step_worker.stop_worker();
    this->run();
    return stopped;
}

bool DevStats::on_reset()
{
    _lanes.clear();
    return true;
}

} // namespace sihd::core

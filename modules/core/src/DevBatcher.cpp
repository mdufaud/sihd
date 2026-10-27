#include <sihd/core/DevBatcher.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

#define BATCH_SUFFIX "_batch"

namespace sihd::core
{

using namespace sihd::util;

SIHD_REGISTER_FACTORY(DevBatcher)

SIHD_LOGGER;

DevBatcher::DevBatcher(const std::string & name, Node *parent):
    sihd::core::Device(name, parent),
    _samples(0),
    _period(0),
    _step_worker(this)
{
    constexpr int default_period_ms = 100;
    this->set_period(default_period_ms);
    this->add_conf("samples", &DevBatcher::set_samples);
    this->add_conf("period", &DevBatcher::set_period);
}

DevBatcher::~DevBatcher()
{
    _step_worker.stop_worker();
}

bool DevBatcher::is_running() const
{
    return _step_worker.is_worker_running();
}

bool DevBatcher::set_samples(size_t samples)
{
    if (samples == 0)
        return false;
    _samples = samples;
    return true;
}

bool DevBatcher::set_period(int milliseconds)
{
    if (milliseconds <= 0)
        return false;
    _period = Duration(time::milli(milliseconds));
    return _step_worker.set_frequency(1000.0 / milliseconds);
}

void DevBatcher::handle(sihd::core::Channel *c)
{
    Sample sample {c, {}, _clock.now()};
    sample.data.resize(c->array()->byte_size());
    if (c->copy_to(sample.data) == false)
        return;
    _pending.push(std::move(sample));
}

bool DevBatcher::run()
{
    this->_accumulate();
    const Timestamp now = _clock.now();
    for (auto & [in, lane] : _lanes)
    {
        if (lane.current.samples > 0 && now - lane.current.arrival >= _period)
            this->_write_current(lane);
    }
    return true;
}

void DevBatcher::_accumulate()
{
    while (std::optional<Sample> sample = _pending.try_pop())
    {
        const auto it = _lanes.find(sample->in);
        if (it == _lanes.end())
            continue;
        Lane & lane = it->second;
        if (lane.current.samples == 0)
            lane.current.arrival = sample->arrival;
        lane.current.data.push_back(sample->data.data(), sample->data.size());
        if (++lane.current.samples >= _samples)
            this->_write_current(lane);
    }
}

void DevBatcher::_write_current(Lane & lane)
{
    if (lane.batch->write(ArrByteView(lane.current.data)) == false)
        SIHD_LOG(warning, "DevBatcher: {} could not write {}", this->name(), lane.batch->name());
    lane.current = Block {};
}

bool DevBatcher::on_start()
{
    if (_samples == 0)
    {
        SIHD_LOG(error, "DevBatcher: {} needs a number of samples", this->name());
        return false;
    }
    auto inputs = this->linked_channels();
    if (SIHD_UNEXPECTED_LOG(inputs))
        return false;

    _lanes.clear();
    _pending.clear();
    for (const auto & [name, in] : *inputs)
    {
        const std::string batch_name = name + BATCH_SUFFIX;
        Channel *batch = this->get_channel(batch_name).value_or(nullptr);
        if (batch == nullptr)
        {
            const size_t block_capacity = _samples * (in->resizable() ? in->capacity() : in->size());
            batch = this->add_channel_resizable(batch_name, in->data_type(), block_capacity, block_capacity);
            if (batch == nullptr)
                return false;
            // every block is an event, even one equal to the previous
            batch->set_write_on_change(false);
        }
        _lanes[in].batch = batch;
    }

    if (_step_worker.start_worker(this->name()) == false)
    {
        SIHD_LOG(error, "DevBatcher: {} could not start its thread", this->name());
        return false;
    }
    for (const auto & [name, in] : *inputs)
        this->observe_channel(in);
    return true;
}

bool DevBatcher::on_stop()
{
    this->remove_channels_observation();
    const bool stopped = _step_worker.stop_worker();
    this->_accumulate();
    for (auto & [in, lane] : _lanes)
    {
        if (lane.current.samples > 0)
            this->_write_current(lane);
    }
    return stopped;
}

bool DevBatcher::on_reset()
{
    _lanes.clear();
    return true;
}

} // namespace sihd::core

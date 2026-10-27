#include <sihd/core/DevWorker.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

#define OUT_SUFFIX "_out"
#define CHANNEL_DROPPED "dropped"

namespace sihd::core
{

using namespace sihd::util;

SIHD_REGISTER_FACTORY(DevWorker)

SIHD_LOGGER;

DevWorker::DevWorker(const std::string & name, Node *parent):
    sihd::core::Device(name, parent),
    _queue_size(0),
    _worker(this),
    _dropped(0),
    _published_dropped(0),
    _publishing(false),
    _channel_dropped_ptr(nullptr)
{
    constexpr size_t default_queue_size = 1024;
    this->set_queue_size(default_queue_size);
    this->add_conf("queue_size", &DevWorker::set_queue_size);
}

DevWorker::~DevWorker()
{
    if (this->is_running())
        this->stop();
}

bool DevWorker::is_running() const
{
    return _worker.is_worker_running();
}

bool DevWorker::set_queue_size(size_t queue_size)
{
    if (queue_size == 0)
        return false;
    _queue_size = queue_size;
    return true;
}

void DevWorker::handle(sihd::core::Channel *c)
{
    Channel *out = this->relay_out(c);
    if (out == nullptr)
        return;

    Item item {out, {}};
    item.data.resize(c->array()->byte_size());
    if (c->copy_to(item.data) == false)
        return;
    if (_queue->push(std::move(item), _queue_size) == false)
        this->count_drop();
}

bool DevWorker::run()
{
    while (true)
    {
        Result<Item> item = _queue->pop();
        if (!item)
            break;
        if (item->out->write(item->data) == false)
            SIHD_LOG(warning, "DevWorker: {} could not write {}", this->name(), item->out->name());
        this->publish_dropped();
    }
    return true;
}

bool DevWorker::on_init()
{
    this->add_unlinked_channel(CHANNEL_DROPPED, TYPE_ULONG, 1);
    return true;
}

bool DevWorker::bind_relays()
{
    auto dropped = this->get_channel(CHANNEL_DROPPED);
    if (SIHD_UNEXPECTED_LOG(dropped))
        return false;
    _channel_dropped_ptr = *dropped;

    auto inputs = this->linked_channels();
    if (SIHD_UNEXPECTED_LOG(inputs))
        return false;
    _out_of.clear();
    for (const auto & [name, in] : *inputs)
    {
        const std::string out_name = name + OUT_SUFFIX;
        Channel *out = this->get_channel(out_name).value_or(nullptr);
        if (out == nullptr)
        {
            out = in->resizable() ? this->add_channel_resizable(out_name, in->data_type(), in->size(), in->capacity())
                                  : this->add_channel(out_name, in->data_type(), in->size());
            if (out == nullptr)
                return false;
            // every write of the source is replayed, even an equal one
            out->set_write_on_change(false);
        }
        _out_of[in] = out;
    }

    _dropped = 0;
    _published_dropped = 0;
    _channel_dropped_ptr->write<uint64_t>(0, 0);
    return true;
}

void DevWorker::observe_relays()
{
    for (const auto & [in, out] : _out_of)
        this->observe_channel(in);
}

Channel *DevWorker::relay_out(Channel *in) const
{
    const auto it = _out_of.find(in);
    return it == _out_of.end() ? nullptr : it->second;
}

const std::map<Channel *, Channel *> & DevWorker::relays() const
{
    return _out_of;
}

size_t DevWorker::queue_size() const
{
    return _queue_size;
}

void DevWorker::count_drop()
{
    ++_dropped;
}

void DevWorker::publish_dropped()
{
    // one writer at a time: two threads writing the same channel would refuse each other's write
    while (_dropped != _published_dropped && _publishing.exchange(true) == false)
    {
        _published_dropped = _dropped.load();
        _channel_dropped_ptr->write<uint64_t>(0, _published_dropped);
        _publishing = false;
    }
}

bool DevWorker::on_start()
{
    if (!this->bind_relays())
        return false;
    _queue.emplace();
    if (_worker.start_sync_worker(this->name()) == false)
    {
        SIHD_LOG(error, "DevWorker: {} could not start its thread", this->name());
        return false;
    }
    this->observe_relays();
    return true;
}

bool DevWorker::on_stop()
{
    this->remove_channels_observation();
    _queue->terminate();
    return _worker.stop_worker();
}

bool DevWorker::on_reset()
{
    _out_of.clear();
    _channel_dropped_ptr = nullptr;
    return true;
}

} // namespace sihd::core

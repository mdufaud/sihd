#include <sihd/core/DevPlayer.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/Task.hpp>

#define CHANNEL_PLAY "play"
#define CHANNEL_END "end"

namespace sihd::core
{

SIHD_UTIL_REGISTER_FACTORY(DevPlayer)

LOGGER;

DevPlayer::DevPlayer(const std::string & name, sihd::util::Node *parent):
    sihd::core::Device(name, parent),
    _running(false),
    _provider_ptr(nullptr),
    _channel_play_ptr(nullptr),
    _channel_end_ptr(nullptr)
{
    _records_queue_limit = 20;
    _worker.set_runnable(new sihd::util::Task([this] () -> bool
    {
        return this->_worker_loop();
    }));
    this->add_conf("provider", &DevPlayer::set_provider);
    this->add_conf("queue_size", &DevPlayer::set_scheduler_queue_size);
    this->add_conf("alias", &DevPlayer::add_alias);
}

DevPlayer::~DevPlayer()
{
}

bool    DevPlayer::is_running() const
{
    return _running;
}

bool    DevPlayer::set_scheduler_queue_size(size_t limit)
{
    _records_queue_limit = limit;
    return true;
}

bool    DevPlayer::set_provider(const std::string & path)
{
    _provider_path = path;
    return true;
}

bool    DevPlayer::add_alias(const std::string & alias_conf)
{
    std::vector<std::string> conf = sihd::util::Str::split(alias_conf, "=");

    if (conf.size() != 2)
    {
        LOG(error, "DevReplayer: wrong alias configuration: '" << alias_conf
                << "' - expected RECORD_CHANNEL_NAME=CHANNEL_PATH");
        return false;
    }
    // conf[0] = record_channel_name
    // conf[1] = channel_path
    _map_channels_alias[conf[0]] = conf[1];
    return true;
}

void    DevPlayer::observable_changed(sihd::core::Channel *c)
{
    if (c == _channel_play_ptr)
    {
        if (_channel_play_ptr->read<bool>(0) == false)
            _scheduler_ptr->pause();
        else
            _scheduler_ptr->resume();
    }
}

bool    DevPlayer::on_init()
{
    _provider_ptr = this->find<sihd::util::IProvider<PlayableRecord &>>(_provider_path);
    if (_provider_ptr == nullptr)
    {
        LOG(error, "DevReplayer: could not find provider: " << _provider_path);
        return false;
    }
    this->add_unlinked_channel(CHANNEL_PLAY, sihd::util::DBOOL, 1);
    this->add_unlinked_channel(CHANNEL_END, sihd::util::DBOOL, 1);
    _scheduler_ptr = this->add_child<sihd::util::Scheduler>("scheduler");
    return true;
}

bool    DevPlayer::on_start()
{
    if (this->get_channel(CHANNEL_PLAY, &_channel_play_ptr) == false)
        return false;
    this->observe_channel(_channel_play_ptr);
    if (this->get_channel(CHANNEL_END, &_channel_end_ptr) == false)
        return false;

    Channel *channel_ptr;
    for (const auto & pair: _map_channels_alias)
    {
        channel_ptr = this->find<Channel>(pair.second);
        if (channel_ptr == nullptr)
        {
            LOG(error, "DevRecorder: channel to record '"
                        << pair.first << "' not found: " << pair.second);
            return false;
        }
        _map_channels[pair.first] = channel_ptr;
    }

    if (_channel_play_ptr->read<bool>(0) == false)
        _scheduler_ptr->pause();
    if (_scheduler_ptr->start() == false)
    {
        LOG(error, "DevReplayer: could not start scheduler");
        return false;
    }
    if (_worker.start_worker(this->get_name()) == false)
    {
        _scheduler_ptr->stop();
        LOG(error, "DevReplayer: could not start worker");
        return false;
    }
    _running = true;
    return true;
}

bool    DevPlayer::run()
{
    {
        std::lock_guard l(_run_mutex);
        if (_running == false)
            return false;
    }
    PlayableRecord & record = _queue.front();
    _queue.pop();
    Channel *c = _map_channels[record.name];
    if (c != nullptr)
        c->write(*record.value);
    _waitable.notify(1);
    if (_last_record && _queue.empty())
        _channel_end_ptr->notify();
    return true;
}

bool    DevPlayer::_worker_loop()
{
    PlayableRecord record;
    time_t begin = _scheduler_ptr->get_clock()->now();
    time_t first = -1;
    time_t execute_at;
    _last_record = false;
    while (_provider_ptr->provide(record) != false)
    {
        _queue.push(record);
        if (first < 0)
            first = record.timestamp;
        execute_at = begin + (record.timestamp - first);
        while (_running && (_queue.size() > _records_queue_limit))
            _waitable.infinite_wait();
        if (_running == false)
            return false;
        _scheduler_ptr->add_task(new sihd::util::Task(this, execute_at));
    }
    _last_record = true;
    return true;
}

bool    DevPlayer::on_stop()
{
    {
        std::lock_guard l(_run_mutex);
        _running = false;
    }
    _waitable.notify_all();
    if (_worker.stop_worker() == false)
        LOG(error, "DevReplayer: could not stop worker");
    if (_scheduler_ptr->stop() == false)
        LOG(error, "DevReplayer: could not stop scheduler");
    _channel_play_ptr = nullptr;
    _channel_end_ptr = nullptr;
    _map_channels.clear();
    return true;
}

bool    DevPlayer::on_reset()
{
    _queue = std::queue<PlayableRecord>();
    _map_channels_alias.clear();
    return true;
}

}
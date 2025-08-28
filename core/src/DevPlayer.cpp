#include <sihd/core/DevPlayer.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/Splitter.hpp>
#include <sihd/util/Task.hpp>

#define CHANNEL_PLAY "play"
#define CHANNEL_END "end"

namespace sihd::core
{

SIHD_UTIL_REGISTER_FACTORY(DevPlayer)

SIHD_LOGGER;

using namespace sihd::util;

DevPlayer::DevPlayer(const std::string & name, sihd::util::Node *parent):
    sihd::core::Device(name, parent),
    _running(false),
    _channel_play_ptr(nullptr),
    _channel_end_ptr(nullptr)
{
    _runnable.set_method(this, &DevPlayer::_main_loop);
    _worker.set_runnable(&_runnable);
    _collector.add_observer(this);
    this->set_provider_wait_time(10);
    this->set_scheduler_queue_size(200);
    this->add_conf("provider", &DevPlayer::set_provider);
    this->add_conf("provider_wait_time", &DevPlayer::set_provider_wait_time);
    this->add_conf("queue_size", &DevPlayer::set_scheduler_queue_size);
    this->add_conf("alias", &DevPlayer::add_alias);
}

DevPlayer::~DevPlayer() = default;

bool DevPlayer::is_running() const
{
    return _running;
}

bool DevPlayer::set_scheduler_queue_size(size_t limit)
{
    _records_queue_limit = limit;
    return true;
}

bool DevPlayer::set_provider(std::string_view path)
{
    _provider_path = path;
    return true;
}

bool DevPlayer::set_provider_wait_time(time_t milliseconds)
{
    if (milliseconds <= 0)
    {
        SIHD_LOG(error, "DevPlayer: cannot wait for {} milliseconds", milliseconds);
        return false;
    }
    _collector.set_timeout(milliseconds);
    return true;
}

bool DevPlayer::add_alias(std::string_view alias_conf)
{
    sihd::util::Splitter splitter("=");
    std::vector<std::string> conf = splitter.split(alias_conf);

    if (conf.size() != 2)
    {
        SIHD_LOG(error,
                 "DevReplayer: wrong alias configuration: '{}' - expected RECORD_CHANNEL_NAME=CHANNEL_PATH",
                 alias_conf);
        return false;
    }
    // conf[0] = record_channel_name
    // conf[1] = channel_path
    _map_channels_alias[conf[0]] = conf[1];
    return true;
}

void DevPlayer::handle(sihd::core::Channel *c)
{
    if (c == _channel_play_ptr)
    {
        if (_channel_play_ptr->read<bool>(0) == false)
            _scheduler_ptr->pause();
        else
            _scheduler_ptr->resume();
    }
}

bool DevPlayer::on_init()
{
    this->add_unlinked_channel(CHANNEL_PLAY, TYPE_BOOL, 1);
    this->add_unlinked_channel(CHANNEL_END, TYPE_BOOL, 1);

    _scheduler_ptr = this->add_child<sihd::util::Scheduler>(fmt::format("{}-scheduler", this->name()));
    if (_scheduler_ptr != nullptr)
    {
        _scheduler_ptr->set_start_synchronised(true);
        _scheduler_ptr->pause();
    }
    return _scheduler_ptr != nullptr;
}

bool DevPlayer::on_start()
{
    // provider
    IProvider<PlayableRecord> *provider = this->find<IProvider<PlayableRecord>>(_provider_path);
    if (provider == nullptr)
    {
        SIHD_LOG(error, "DevReplayer: could not find provider: {}", _provider_path);
        return false;
    }
    _collector.set_provider(provider);

    // channel play
    if (this->get_channel(CHANNEL_PLAY, &_channel_play_ptr) == false)
        return false;
    this->observe_channel(_channel_play_ptr);

    // channel end
    if (this->get_channel(CHANNEL_END, &_channel_end_ptr) == false)
        return false;
    _channel_end_ptr->write<bool>(0, false);

    // channels to play to
    Channel *channel_ptr;
    for (const auto & pair : _map_channels_alias)
    {
        channel_ptr = this->find<Channel>(pair.second);
        if (channel_ptr == nullptr)
        {
            SIHD_LOG(error, "DevRecorder: channel to record '{}' not found: {}", pair.first, pair.second);
            return false;
        }
        _map_channels[pair.first] = channel_ptr;
    }

    // check if must play
    if (_channel_play_ptr->read<bool>(0) == true)
        _scheduler_ptr->resume();

    // start thread
    _running = true;
    if (_worker.start_worker(this->name()) == false)
    {
        _running = false;
        _scheduler_ptr->stop();
        SIHD_LOG(error, "DevReplayer: could not start worker");
    }
    return _running;
}

bool DevPlayer::run()
{
    if (_running == false)
        return false;

    PlayableRecord record = _safe_queue.front();
    _safe_queue.pop();

    Channel *c = _map_channels[record.name];
    if (c != nullptr)
        c->write(*record.value.get());
    else
        SIHD_LOG(error, "DevPlayer: channel '{}' not found", record.name);

    {
        auto l = _waitable.guard();
        _waitable.notify();
    }
    if (_last_record)
        this->_provider_ended();
    return true;
}

void DevPlayer::handle(Collector<PlayableRecord> *collector)
{
    if (_last_record)
        return;

    PlayableRecord record = collector->data();
    _safe_queue.push(record);

    if (_first_timestamp.has_value() == false)
        _first_timestamp = record.timestamp;

    _waitable.wait([this] { return _running == false || _safe_queue.size() <= _records_queue_limit; });
    if (_running == false)
        return;

    // calls DevPlayer::run to execute record at setted time
    _scheduler_ptr->add_task(new Task(this, {.run_in = record.timestamp - _first_timestamp.value()}));
}

bool DevPlayer::_main_loop()
{
    // _channel_end_ptr->write<bool>(0, false);

    _first_timestamp.reset();
    _last_record = false;
    // run collector loop which calls DevPlayer::handle for each record
    const bool collector_started = _collector.start();
    // last record to be played in scheduler thread can have no next record
    _last_record = true;
    this->_provider_ended();
    return collector_started;
}

void DevPlayer::_provider_ended()
{
    // if no task in queue - notify end of player
    if (_safe_queue.empty())
        _channel_end_ptr->write<bool>(0, true);
}

bool DevPlayer::on_stop()
{
    {
        auto l = _waitable.guard();
        _running = false;
        _waitable.notify();
    }
    if (_worker.stop_worker() == false)
        SIHD_LOG(error, "DevReplayer: could not stop worker");
    _channel_play_ptr = nullptr;
    _channel_end_ptr = nullptr;
    _collector.stop();
    _collector.set_provider(nullptr);
    _map_channels.clear();
    return true;
}

bool DevPlayer::on_reset()
{
    _safe_queue.clear();
    _channel_play_ptr = nullptr;
    _channel_end_ptr = nullptr;
    _scheduler_ptr = nullptr;
    _map_channels_alias.clear();
    return true;
}

} // namespace sihd::core

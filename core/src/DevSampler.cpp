#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/Splitter.hpp>

#include <sihd/core/DevSampler.hpp>

#define CHANNEL_SAMPLE "sample"

namespace sihd::core
{

SIHD_UTIL_REGISTER_FACTORY(DevSampler)

SIHD_LOGGER;

DevSampler::DevSampler(const std::string & name, sihd::util::Node *parent):
    sihd::core::Device(name, parent),
    _channel_sample(nullptr)
{
    _step_worker.set_runnable(this);
    this->add_conf("frequency", &DevSampler::set_frequency);
    this->add_conf("sample", &DevSampler::set_sample);
}

DevSampler::~DevSampler() {}

bool DevSampler::set_frequency(double freq)
{
    bool ret = _step_worker.set_frequency(freq);
    if (!ret)
        SIHD_LOG(error, "DevSampler: cannot set frequency: {}", freq);
    return ret;
}

bool DevSampler::set_sample(std::string_view conf)
{
    sihd::util::Splitter splitter("=");
    std::vector<std::string> splitted = splitter.split(conf);

    if (splitted.size() != 2)
    {
        SIHD_LOG(
            error,
            "DevSampler: wrong sampling configuration: '{}' - expected CHANNEL_PATH_SAMPLE_OUT=CHANNEL_PATH_SAMPLE_IN",
            conf);
        return false;
    }
    // splitted[0] = channel_path_out
    // splitted[1] = channel_path_in
    _conf_map[splitted[0]] = splitted[1];
    return true;
}

bool DevSampler::is_running() const
{
    return _step_worker.is_worker_running();
}

void DevSampler::handle(sihd::core::Channel *channel)
{
    std::lock_guard l(_set_mutex);
    if (_channels_map.find(channel) != _channels_map.end())
    {
        _channels_sample_set.emplace(channel);
    }
}

bool DevSampler::on_setup()
{
    return true;
}

bool DevSampler::on_init()
{
    Channel *sample = this->add_unlinked_channel(CHANNEL_SAMPLE, sihd::util::TYPE_BOOL, 1);
    if (sample != nullptr)
        sample->set_write_on_change(false);
    return true;
}

bool DevSampler::on_start()
{
    bool ret;
    Channel *channel_in;
    Channel *channel_out;

    ret = true;
    if (!this->get_channel(CHANNEL_SAMPLE, &_channel_sample))
        return false;
    for (const auto & [channel_out_path, channel_in_path] : _conf_map)
    {
        if (this->find_channel(channel_in_path, &channel_in) && this->find_channel(channel_out_path, &channel_out))
        {
            if (this->observe_channel(channel_in) == false)
                ret = false;
            _channels_map[channel_in] = channel_out;
        }
        else
            ret = false;
    }
    if (ret && _step_worker.start_sync_worker(this->name()) == false)
    {
        SIHD_LOG(error, "DevSampler: could not start worker");
        return false;
    }
    return ret;
}

bool DevSampler::run()
{
    /*
        copying set so we can get new notifications while processing the old ones
        channels to sample to may have long processing of time and taking mutex ownership
        means missing notifications
    */
    std::set<Channel *> channels_set;
    {
        std::lock_guard l(_set_mutex);
        channels_set = _channels_sample_set;
        _channels_sample_set.clear();
    }
    Channel *channel_out;
    for (Channel *channel_in : channels_set)
    {
        channel_out = _channels_map[channel_in];
        if (channel_out->write(*channel_in) == false)
        {
            SIHD_LOG(error, "DevSampler: Could not sample into: {}", channel_out->name());
        }
    }
    if (channels_set.size() > 0)
        _channel_sample->write(0, true);
    return true;
}

bool DevSampler::on_stop()
{
    if (_step_worker.stop_worker() == false)
        SIHD_LOG(error, "DevSampler: could not stop worker");
    _channels_map.clear();
    {
        std::lock_guard l(_set_mutex);
        _channels_sample_set.clear();
    }
    return true;
}

bool DevSampler::on_reset()
{
    _conf_map.clear();
    return true;
}

} // namespace sihd::core

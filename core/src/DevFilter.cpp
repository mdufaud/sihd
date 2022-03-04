#include <sihd/core/DevFilter.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

namespace sihd::core
{

SIHD_UTIL_REGISTER_FACTORY(DevFilter)

SIHD_LOGGER;

DevFilter::DevFilter(const std::string & name, sihd::util::Node *parent):
    sihd::core::Device(name, parent), _running(false)
{
    this->add_conf("filter_equal", &DevFilter::set_filter_equal);
}

DevFilter::~DevFilter()
{
}

bool    DevFilter::_parse_conf(const std::string & conf_str, DevFilter::RuleConf & rule_conf)
{
    // in=channel_path_in;out=channel_path_out;trigger=i:val1;write=j:val2
    std::map<std::string, std::string> parsed_conf = sihd::util::Str::parse_configuration(conf_str);

    auto channel_in_iterator = parsed_conf.find("in");
    if (channel_in_iterator == parsed_conf.end())
    {
        SIHD_LOG(error, "DevFilter: no channel input 'in' in configuration: " << conf_str);
        return false;
    }
    auto channel_out_iterator = parsed_conf.find("out");
    if (channel_out_iterator == parsed_conf.end())
    {
        SIHD_LOG(error, "DevFilter: no channel output 'out' in configuration: " << conf_str);
        return false;
    }
    auto trigger_iterator = parsed_conf.find("trigger");
    if (trigger_iterator == parsed_conf.end())
    {
        SIHD_LOG(error, "DevFilter: no trigger value 'trigger' in configuration: " << conf_str);
        return false;
    }
    rule_conf.channel_in_path = channel_in_iterator->second;
    rule_conf.channel_out_path = channel_out_iterator->second;
    rule_conf.trigger = trigger_iterator->second;

    auto write_iterator = parsed_conf.find("write");
    if (write_iterator != parsed_conf.end())
        rule_conf.write = write_iterator->second;
    return true;
}

bool    DevFilter::set_filter_equal(const std::string & conf_str)
{
    RuleConf rule_conf;
    rule_conf.type = equal;
    return this->_parse_conf(conf_str, rule_conf);
}

bool    DevFilter::is_running() const
{
    return _running;
}

void    DevFilter::_matched(Channel *channel_out, DevFilter::Rule *rule, int64_t out_val)
{
    sihd::util::IArray *array_out = channel_out->array();
    memcpy((void *)(array_out->buf() + (rule->write_idx * array_out->data_size())),
            (const void *)&out_val,
            array_out->data_size());
    channel_out->notify();
}

void    DevFilter::_handle_float(Channel *channel_in, Rule *rule_ptr)
{
    (void)channel_in;
    (void)rule_ptr;
}

void    DevFilter::_handle_int(Channel *channel_in, Rule *rule_ptr)
{
    const sihd::util::IArray *array_in = channel_in->carray();
    int64_t in_val = 0;
    memcpy((void *)&in_val,
        (const void *)(array_in->cbuf() + (rule_ptr->trigger_idx * array_in->data_size())),
        array_in->data_size());
    bool matched = false;
    switch (rule_ptr->type)
    {
        case superior:
        {
            matched = in_val > rule_ptr->trigger_value;
            break ;
        }
        case equal:
        {
            matched = in_val == rule_ptr->trigger_value;
            break ;
        }
        default:
            break ;
    }
    if (matched)
    {
        int64_t out_val = rule_ptr->write_same_value ? in_val : rule_ptr->write_value;
        this->_matched(rule_ptr->channel_out_ptr, rule_ptr, out_val);
    }
}

void    DevFilter::handle(sihd::core::Channel *channel)
{
    auto it = _rules_map.find(channel);
    if (it == _rules_map.end())
        return ;
    Rule *rule_ptr = it->second.get();
    if (channel->carray()->data_type() == sihd::util::TYPE_FLOAT
            || channel->carray()->data_type() == sihd::util::TYPE_DOUBLE)
        this->_handle_float(channel, rule_ptr);
    else
        this->_handle_int(channel, rule_ptr);
}

bool    DevFilter::on_setup()
{
    return true;
}

bool    DevFilter::on_init()
{
    return true;
}

bool    DevFilter::on_start()
{
    bool ret;
    Channel *channel_in;
    Channel *channel_out;

    ret = true;
    for (const RuleConf & conf: _rules_lst)
    {
        if (this->find_channel(conf.channel_in_path, &channel_in)
                && this->find_channel(conf.channel_out_path, &channel_out))
        {
            if (this->observe_channel(channel_in) == false)
                ret = false;
            std::unique_ptr<Rule> rule(new Rule());
            if (rule.get()->parse(conf, channel_in, channel_out) == false)
                _rules_map[channel_in] = std::move(rule);
            else
                ret = false;
        }
        else
            ret = false;
    }
    _running = ret;
    return ret;
}

bool    DevFilter::on_stop()
{
    {
        std::lock_guard l(_run_mutex);
        _running = false;
    }
    _rules_map.clear();
    return true;
}

bool    DevFilter::on_reset()
{
    _rules_lst.clear();
    return true;
}

DevFilter::Rule::Rule():
    channel_in_ptr(nullptr), channel_out_ptr(nullptr),
    write_same_value(true), trigger_idx(0), trigger_value(0),
    write_idx(0), write_value(0)
{
}

DevFilter::Rule::~Rule()
{
}

bool    DevFilter::Rule::parse(const DevFilter::RuleConf & conf, Channel *in, Channel *out)
{
    if (conf.trigger.empty())
    {
        return false;
    }
    sihd::util::Splitter splitter(":");
    splitter.set_empty_delimitations(true);
    std::vector<std::string> split_trigger = splitter.split(conf.trigger);
    // conf -> trigger
    if (split_trigger.size() == 0 || split_trigger.size() > 2)
    {
        SIHD_LOG(error, "DevFilter: trigger conf error: " << conf.trigger);
        return false;
    }
    if (split_trigger.size() == 1)
    {
        // conf -> trigger=value
        if (split_trigger[0].empty())
        {
            SIHD_LOG(error, "DevFilter: trigger value empty: '" << conf.trigger << "'");
            return false;
        }
        this->trigger_idx = 0;
        if (sihd::util::Str::convert_from_string<int64_t>(split_trigger[0], this->trigger_value) == false)
        {
            SIHD_LOG(error, "DevFilter: cannot convert trigger value: " << split_trigger[0]);
            return false;
        }
    }
    else
    {
        // conf -> trigger=index:value
        if (split_trigger[0].empty() && split_trigger[1].empty())
        {
            SIHD_LOG(error, "DevFilter: trigger idx and value empty: '" << conf.trigger << "'");
            return false;
        }
        if (split_trigger[0].empty() == false
                && sihd::util::Str::convert_from_string<size_t>(split_trigger[0], this->trigger_idx) == false)
        {
            SIHD_LOG(error, "DevFilter: cannot convert trigger idx: " << split_trigger[0]);
            return false;
        }
        if (split_trigger[1].empty() == false
                && sihd::util::Str::convert_from_string<int64_t>(split_trigger[1], this->trigger_value) == false)
        {
            SIHD_LOG(error, "DevFilter: cannot convert trigger value: " << split_trigger[1]);
            return false;
        }
    }
    // conf -> write
    if (conf.write.empty())
    {
        this->write_idx = this->trigger_idx;
        this->write_same_value = true;
    }
    else
    {
        std::vector<std::string> split_write = splitter.split(conf.write);
        if (split_write.size() == 0 || split_write.size() > 2)
        {
            SIHD_LOG(error, "DevFilter: write conf error: " << conf.write);
            return false;
        }
        if (split_write.size() == 1)
        {
            // conf -> write=value
            if (split_write[0].empty())
            {
                SIHD_LOG(error, "DevFilter: write value empty: '" << conf.write << "'");
                return false;
            }
            this->write_idx = this->trigger_idx;
            this->write_same_value = true;
            if (sihd::util::Str::convert_from_string<int64_t>(split_write[0], this->trigger_value) == false)
            {
                SIHD_LOG(error, "DevFilter: cannot convert trigger value: " << split_write[0]);
                return false;
            }
        }
        else
        {
            // conf -> write=index:value
            if (split_write[0].empty() && split_write[1].empty())
            {
                SIHD_LOG(error, "DevFilter: write idx and value empty: '" << conf.write << "'");
                return false;
            }
            if (split_write[0].empty() == false
                    && sihd::util::Str::convert_from_string<size_t>(split_write[0], this->trigger_idx) == false)
            {
                SIHD_LOG(error, "DevFilter: cannot convert write idx: " << split_write[0]);
                return false;
            }
            if (split_write[1].empty() == false
                    && sihd::util::Str::convert_from_string<int64_t>(split_write[1], this->trigger_value) == false)
            {
                SIHD_LOG(error, "DevFilter: cannot convert write value: " << split_write[1]);
                return false;
            }
        }
    }
    this->channel_in_ptr = in;
    this->channel_out_ptr = out;
    return true;
}

}
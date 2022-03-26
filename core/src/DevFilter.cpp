#include <sihd/core/DevFilter.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

#define CONF_SETTINGS_DELIMITER ";"
#define CONF_SETTER_DELIMITER "="
#define CONF_INDEX_DELIMITER "="

#define CONF_KEY_CHANNEL_IN "in"
#define CONF_KEY_CHANNEL_OUT "out"
#define CONF_KEY_TRIGGER "trigger"
#define CONF_KEY_WRITE "write"
#define CONF_KEY_MATCH "match"
#define CONF_KEY_NOTIFY_SAME "notify_same"
#define CONF_KEY_DELAY "delay"

namespace sihd::core
{

SIHD_UTIL_REGISTER_FACTORY(DevFilter)

SIHD_LOGGER;

DevFilter::DevFilter(const std::string & name, sihd::util::Node *parent):
    sihd::core::Device(name, parent), _running(false), _scheduler_ptr(nullptr), _rule_with_delay(false)
{
    this->add_conf("filter_equal", &DevFilter::set_filter_equal);
    this->add_conf("filter_superior", &DevFilter::set_filter_superior);
    this->add_conf("filter_superior_equal", &DevFilter::set_filter_superior_equal);
    this->add_conf("filter_inferior", &DevFilter::set_filter_inferior);
    this->add_conf("filter_inferior_equal", &DevFilter::set_filter_inferior_equal);
    this->add_conf("filter_byte_and", &DevFilter::set_filter_byte_and);
    this->add_conf("filter_byte_or", &DevFilter::set_filter_byte_or);
    this->add_conf("filter_byte_xor", &DevFilter::set_filter_byte_xor);
}

DevFilter::~DevFilter()
{
}

bool    DevFilter::_parse_conf(const std::string & rule_str, RuleType type)
{
    // in=channel_path_in;out=channel_path_out;trigger=i:val1;write=j:val2
    Rule rule(type);
    bool ret = rule.parse(rule_str);
    if (ret)
        this->set_filter(rule);
    return ret;
}

void    DevFilter::set_filter(const Rule & rule)
{
    if (rule.nano_delay > 0)
        _rule_with_delay = true;
    _rules_lst.push_back(rule);
}

bool    DevFilter::set_filter_equal(const std::string & rule_str)
{
    return this->_parse_conf(rule_str, EQUAL);
}

bool    DevFilter::set_filter_superior(const std::string & rule_str)
{
    return this->_parse_conf(rule_str, SUPERIOR);
}

bool    DevFilter::set_filter_superior_equal(const std::string & rule_str)
{
    return this->_parse_conf(rule_str, SUPERIOR_EQUAL);
}

bool    DevFilter::set_filter_inferior(const std::string & rule_str)
{
    return this->_parse_conf(rule_str, INFERIOR);
}

bool    DevFilter::set_filter_inferior_equal(const std::string & rule_str)
{
    return this->_parse_conf(rule_str, INFERIOR_EQUAL);
}

bool    DevFilter::set_filter_byte_and(const std::string & rule_str)
{
    return this->_parse_conf(rule_str, BYTE_AND);
}

bool    DevFilter::set_filter_byte_or(const std::string & rule_str)
{
    return this->_parse_conf(rule_str, BYTE_OR);
}

bool    DevFilter::set_filter_byte_xor(const std::string & rule_str)
{
    return this->_parse_conf(rule_str, BYTE_XOR);
}

void    DevFilter::_rule_match(Channel *channel_out, const Rule *rule_ptr, int64_t out_val)
{
    sihd::util::IArray *array_out = channel_out->array();
    if (rule_ptr->notify_if_same
            || memcmp((void *)(array_out->buf_at(rule_ptr->write_idx)), (const void *)&out_val, array_out->data_size()) != 0)
    {
        memcpy((void *)(array_out->buf_at(rule_ptr->write_idx)),
                (const void *)&out_val,
                array_out->data_size());
        channel_out->notify();
    }
}

void    DevFilter::_apply_rule(const sihd::core::Channel *channel_in, sihd::core::Channel *channel_out, const Rule *rule_ptr)
{
    const sihd::util::IArray *array_in = channel_in->carray();
    sihd::util::Value in_value(array_in->cbuf_at(rule_ptr->trigger_idx), array_in->data_type());
    bool matched = false;
    switch (rule_ptr->type)
    {
        case EQUAL:
            matched = in_value == rule_ptr->trigger_value;
            break ;
        case SUPERIOR:
            matched = in_value > rule_ptr->trigger_value;
            break ;
        case SUPERIOR_EQUAL:
            matched = in_value >= rule_ptr->trigger_value;
            break ;
        case INFERIOR:
            matched = in_value < rule_ptr->trigger_value;
            break ;
        case INFERIOR_EQUAL:
            matched = in_value <= rule_ptr->trigger_value;
            break ;
        case BYTE_AND:
            matched = in_value.data & rule_ptr->trigger_value.data;
            break ;
        case BYTE_OR:
            matched = in_value.data | rule_ptr->trigger_value.data;
            break ;
        case BYTE_XOR:
            matched = in_value.data ^ rule_ptr->trigger_value.data;
            break ;
        default:
            break ;
    }
    if (rule_ptr->should_match == matched)
    {
        int64_t out_val = rule_ptr->write_same_value
                            ? in_value.data
                            : rule_ptr->write_value.data;
        if (rule_ptr->nano_delay > 0 && _scheduler_ptr != nullptr)
            _scheduler_ptr->add_task(new DelayWriter(this, channel_out, rule_ptr, out_val, _scheduler_ptr));
        else
            this->_rule_match(channel_out, rule_ptr, out_val);
    }
}

void    DevFilter::handle(sihd::core::Channel *channel)
{
    auto it = _rules_map.find(channel);
    if (it == _rules_map.end())
        return ;
    for (const std::unique_ptr<InternalRule> & rule_uptr: it->second)
    {
        this->_apply_rule(channel, rule_uptr.get()->channel_out_ptr, rule_uptr.get()->rule_ptr);
    }
}

bool    DevFilter::is_running() const
{
    return _running;
}

bool    DevFilter::on_setup()
{
    return true;
}

bool    DevFilter::on_init()
{
    if (_rule_with_delay)
    {
        _scheduler_ptr = this->add_child<sihd::util::Scheduler>("delay_scheduler");
    }
    return true;
}

bool    DevFilter::on_start()
{
    bool ret;
    Channel *channel_in;
    Channel *channel_out;

    ret = true;
    for (const Rule & conf: _rules_lst)
    {
        if (this->find_channel(conf.channel_in, &channel_in)
                && this->find_channel(conf.channel_out, &channel_out))
        {
            std::unique_ptr<InternalRule> rule(new InternalRule());
            if (rule.get()->set(&conf, channel_in, channel_out))
                _rules_map[channel_in].push_back(std::move(rule));
            else
                ret = false;
            ret = ret && this->observe_channel(channel_in);
        }
        else
            ret = false;
    }
    if (_scheduler_ptr != nullptr && _scheduler_ptr->start() == false)
    {
        SIHD_LOG(error, "DevFilter: could not start scheduler");
        return false;
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
    if (_scheduler_ptr != nullptr && _scheduler_ptr->stop() == false)
    {
        SIHD_LOG(error, "DevFilter: could not stop scheduler");
        return false;
    }
    _rules_map.clear();
    return true;
}

bool    DevFilter::on_reset()
{
    _rules_lst.clear();
    _rule_with_delay = false;
    _scheduler_ptr = nullptr;
    return true;
}

/* ************************************************************************* */
/* DevFilter::Rule */
/* ************************************************************************* */


DevFilter::Rule::Rule(RuleType type):
    type(type),
    trigger_idx(0), trigger_value(0),
    write_same_value(true), write_idx(0), write_value(0),
    notify_if_same(false), should_match(true),
    nano_delay(0)
{
}

DevFilter::Rule::~Rule()
{
}

DevFilter::Rule &   DevFilter::Rule::in(std::string_view channel_name)
{
    this->channel_in = channel_name;
    return *this;
}

DevFilter::Rule &   DevFilter::Rule::out(std::string_view channel_name)
{
    this->channel_out = channel_name;
    return *this;
}

DevFilter::Rule &   DevFilter::Rule::notify_same(bool active)
{
    this->notify_if_same = active;
    return *this;
}

DevFilter::Rule &   DevFilter::Rule::match(bool active)
{
    this->should_match = active;
    return *this;
}

DevFilter::Rule &   DevFilter::Rule::write_same()
{
    this->write_same_value = true;
    this->write_idx = this->trigger_idx;
    return *this;
}

DevFilter::Rule &   DevFilter::Rule::write_same(size_t idx)
{
    this->write_same_value = true;
    this->write_idx = idx;
    return *this;
}

DevFilter::Rule &   DevFilter::Rule::delay(double delay)
{
    this->nano_delay = sihd::util::time::from_double(delay);
    return *this;
}

DevFilter::Rule &   DevFilter::Rule::delay(time_t nano_delay)
{
    this->nano_delay = nano_delay;
    return *this;
}

bool    DevFilter::Rule::parse(const std::string & conf_str)
{
    std::map<std::string, std::string> rule_conf_map = sihd::util::Str::parse_configuration(conf_str);
    if (rule_conf_map.find(CONF_KEY_CHANNEL_IN) == rule_conf_map.end())
    {
        SIHD_LOG_ERROR("DevFilter: no channel input '%s' in configuration: %s", CONF_KEY_CHANNEL_IN, conf_str.c_str());
        return false;
    }
    if (rule_conf_map.find(CONF_KEY_CHANNEL_OUT) == rule_conf_map.end())
    {
        SIHD_LOG_ERROR("DevFilter: no channel output '%s' in configuration: %s", CONF_KEY_CHANNEL_OUT, conf_str.c_str());
        return false;
    }
    if (rule_conf_map.find(CONF_KEY_TRIGGER) == rule_conf_map.end())
    {
        SIHD_LOG_ERROR("DevFilter: no trigger value '%s' in configuration: %s", CONF_KEY_TRIGGER, conf_str.c_str());
        return false;
    }
    this->channel_in = rule_conf_map.at(CONF_KEY_CHANNEL_IN);
    this->channel_out = rule_conf_map.at(CONF_KEY_CHANNEL_OUT);
    return this->_parse_trigger_config(rule_conf_map)
            && this->_parse_write_config(rule_conf_map)
            && this->_parse_options_config(rule_conf_map);
}

bool    DevFilter::Rule::_parse_options_config(const std::map<std::string, std::string> & conf_map)
{
    if (conf_map.find(CONF_KEY_MATCH) != conf_map.end())
    {
        if (sihd::util::Str::convert_from_string<bool>(conf_map.at(CONF_KEY_MATCH), this->should_match) == false)
        {
            SIHD_LOG_ERROR("DevFilter: conf error for '%s': %s", CONF_KEY_MATCH, conf_map.at(CONF_KEY_MATCH).c_str());
            return false;
        }
    }
    if (conf_map.find(CONF_KEY_NOTIFY_SAME) != conf_map.end())
    {
        if (sihd::util::Str::convert_from_string<bool>(conf_map.at(CONF_KEY_NOTIFY_SAME), this->notify_if_same) == false)
        {
            SIHD_LOG_ERROR("DevFilter: conf error for '%s': %s", CONF_KEY_NOTIFY_SAME, conf_map.at(CONF_KEY_NOTIFY_SAME).c_str());
            return false;
        }
    }
    if (conf_map.find(CONF_KEY_DELAY) != conf_map.end())
    {
        double delay;
        if (sihd::util::Str::convert_from_string<double>(conf_map.at(CONF_KEY_DELAY), delay) == false)
        {
            SIHD_LOG_ERROR("DevFilter: conf error for '%s': %s", CONF_KEY_DELAY, conf_map.at(CONF_KEY_DELAY).c_str());
            return false;
        }
        this->nano_delay = sihd::util::time::from_double(delay);
    }
    return true;
}

bool    DevFilter::Rule::_parse_trigger_config(const std::map<std::string, std::string> & conf_map)
{
    const std::string & conf_trigger = conf_map.at(CONF_KEY_TRIGGER);
    sihd::util::Splitter splitter(":");
    splitter.set_empty_delimitations(true);
    std::vector<std::string> split_trigger = splitter.split(conf_trigger);
    // conf -> trigger
    if (split_trigger.size() == 0 || split_trigger.size() > 2)
    {
        SIHD_LOG(error, "DevFilter: trigger conf error: " << conf_trigger);
        return false;
    }
    if (split_trigger.size() == 1)
    {
        // conf -> trigger=value
        if (split_trigger[0].empty())
        {
            SIHD_LOG(error, "DevFilter: trigger value empty: '" << conf_trigger << "'");
            return false;
        }
        this->trigger_idx = 0;
        if (this->trigger_value.from_any_string(split_trigger[0]) == false)
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
            SIHD_LOG(error, "DevFilter: trigger idx and value empty: '" << conf_trigger << "'");
            return false;
        }
        if (split_trigger[0].empty() == false
                && sihd::util::Str::convert_from_string<size_t>(split_trigger[0], this->trigger_idx) == false)
        {
            SIHD_LOG(error, "DevFilter: cannot convert trigger idx: " << split_trigger[0]);
            return false;
        }
        if (split_trigger[1].empty() == false && this->trigger_value.from_any_string(split_trigger[1]) == false)
        {
            SIHD_LOG(error, "DevFilter: cannot convert trigger value: " << split_trigger[1]);
            return false;
        }
    }
    return true;
}

bool    DevFilter::Rule::_parse_write_config(const std::map<std::string, std::string> & conf_map)
{
    if (conf_map.find(CONF_KEY_WRITE) == conf_map.end())
    {
        this->write_idx = this->trigger_idx;
        this->write_same_value = true;
        return true;
    }
    const std::string & conf_write = conf_map.at(CONF_KEY_WRITE);
    sihd::util::Splitter splitter(":");
    splitter.set_empty_delimitations(true);
    std::vector<std::string> split_write = splitter.split(conf_write);
    if (split_write.size() == 0 || split_write.size() > 2)
    {
        SIHD_LOG(error, "DevFilter: write conf error: " << conf_write);
        return false;
    }
    if (split_write.size() == 1)
    {
        // conf -> write=value
        if (split_write[0].empty())
        {
            SIHD_LOG(error, "DevFilter: write value empty: '" << conf_write << "'");
            return false;
        }
        this->write_idx = this->trigger_idx;
        this->write_same_value = false;
        if (this->write_value.from_any_string(split_write[0]) == false)
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
            SIHD_LOG(error, "DevFilter: write idx and value empty: '" << conf_write << "'");
            return false;
        }
        if (split_write[0].empty() == false
                && sihd::util::Str::convert_from_string<size_t>(split_write[0], this->write_idx) == false)
        {
            SIHD_LOG(error, "DevFilter: cannot convert write idx: " << split_write[0]);
            return false;
        }
        this->write_same_value = split_write[1].empty();
        if (this->write_same_value == false && this->write_value.from_any_string(split_write[1]) == false)
        {
            SIHD_LOG(error, "DevFilter: cannot convert write value: " << split_write[1]);
            return false;
        }
    }
    return true;
}

/* ************************************************************************* */
/* DevFilter::DelayWriter */
/* ************************************************************************* */

DevFilter::DelayWriter::DelayWriter(DevFilter *dev, Channel *channel_out, const Rule *rule_ptr, int64_t out_val, sihd::util::Scheduler *scheduler_ptr):
    sihd::util::Task(this, scheduler_ptr->get_clock()->now() + rule_ptr->nano_delay, 0),
    dev(dev), channel_out(channel_out), rule_ptr(rule_ptr), out_val(out_val)
{
}

DevFilter::DelayWriter::~DelayWriter()
{
}

bool    DevFilter::DelayWriter::run()
{
    this->dev->_rule_match(this->channel_out, this->rule_ptr, this->out_val);
    return true;
}

/* ************************************************************************* */
/* DevFilter::InternalRule */
/* ************************************************************************* */

DevFilter::InternalRule::InternalRule(): channel_in_ptr(nullptr), channel_out_ptr(nullptr), rule_ptr(nullptr)
{
}

DevFilter::InternalRule::~InternalRule()
{
}

bool    DevFilter::InternalRule::set(const DevFilter::Rule *conf, Channel *in, Channel *out)
{
    if (in == out)
    {
        SIHD_LOG_ERROR("DevFilter: config error, channel input '%s' and output '%s' are the same",
                        conf->channel_in.c_str(), conf->channel_out.c_str());
        return false;
    }
    this->channel_in_ptr = in;
    this->channel_out_ptr = out;
    this->rule_ptr = conf;
    return this->verify();
}

bool    DevFilter::InternalRule::verify()
{
    // check if index will be good
    if (rule_ptr->trigger_idx >= this->channel_in_ptr->array()->size())
    {
        SIHD_LOG_ERROR("DevFilter: trigger index %lu is higher or equal than channel input '%s' size %lu",
                        rule_ptr->trigger_idx, rule_ptr->channel_in.c_str(), this->channel_in_ptr->array()->size());
        return false;
    }
    if (rule_ptr->write_idx >= this->channel_out_ptr->array()->size())
    {
        SIHD_LOG_ERROR("DevFilter: write index %lu is higher or equal than channel output '%s' size %lu",
                        rule_ptr->write_idx, rule_ptr->channel_out.c_str(), this->channel_out_ptr->array()->size());
        return false;
    }
    bool in_array_is_float = this->channel_in_ptr->array()->data_type() == sihd::util::TYPE_FLOAT
                            || this->channel_in_ptr->array()->data_type() == sihd::util::TYPE_DOUBLE;
    // check if trigger value type against channel
    if (rule_ptr->trigger_value.is_float() && in_array_is_float == false)
    {
        SIHD_LOG_ERROR("DevFilter: type error, trigger value is float and channel input '%s' is not a floating type",
                        this->channel_in_ptr->get_name().c_str());
        return false;
    }
    // check write value type against channel
    bool out_array_is_float = this->channel_out_ptr->array()->data_type() == sihd::util::TYPE_FLOAT
                                || this->channel_out_ptr->array()->data_type() == sihd::util::TYPE_DOUBLE;
    if (out_array_is_float == false
            && ((rule_ptr->write_same_value && rule_ptr->trigger_value.is_float())
                || (rule_ptr->write_same_value == false && rule_ptr->write_value.is_float())))
    {
        SIHD_LOG_ERROR("DevFilter: type error, write value is float and channel output '%s' is not a floating type",
                        this->channel_out_ptr->get_name().c_str());
        return false;
    }
    return true;
}

}
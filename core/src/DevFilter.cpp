#include <sihd/core/DevFilter.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

#define CONF_KEY_TRIGGER "trigger"
#define CONF_KEY_CHANNEL_IN "in"
#define CONF_KEY_CHANNEL_OUT "out"
#define CONF_KEY_WRITE "write"
#define CONF_KEY_MATCH "match"

namespace sihd::core
{

SIHD_UTIL_REGISTER_FACTORY(DevFilter)

SIHD_LOGGER;

DevFilter::DevFilter(const std::string & name, sihd::util::Node *parent):
    sihd::core::Device(name, parent), _running(false)
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

bool    DevFilter::_parse_conf(const std::string & conf_str, DevFilter::RuleConf & rule_conf)
{
    // in=channel_path_in;out=channel_path_out;trigger=i:val1;write=j:val2
    rule_conf.conf_map = sihd::util::Str::parse_configuration(conf_str);

    if (rule_conf.conf_map.find(CONF_KEY_CHANNEL_IN) == rule_conf.conf_map.end())
    {
        SIHD_LOG(error, "DevFilter: no channel input 'in' in configuration: " << conf_str);
        return false;
    }
    if (rule_conf.conf_map.find(CONF_KEY_CHANNEL_OUT) == rule_conf.conf_map.end())
    {
        SIHD_LOG(error, "DevFilter: no channel output 'out' in configuration: " << conf_str);
        return false;
    }
    if (rule_conf.conf_map.find(CONF_KEY_TRIGGER) == rule_conf.conf_map.end())
    {
        SIHD_LOG(error, "DevFilter: no trigger value 'trigger' in configuration: " << conf_str);
        return false;
    }
    _rules_lst.push_back(rule_conf);
    return true;
}

bool    DevFilter::set_filter_equal(const std::string & conf_str)
{
    RuleConf rule_conf;
    rule_conf.type = EQUAL;
    return this->_parse_conf(conf_str, rule_conf);
}

bool    DevFilter::set_filter_superior(const std::string & conf_str)
{
    RuleConf rule_conf;
    rule_conf.type = SUPERIOR;
    return this->_parse_conf(conf_str, rule_conf);
}

bool    DevFilter::set_filter_superior_equal(const std::string & conf_str)
{
    RuleConf rule_conf;
    rule_conf.type = SUPERIOR_EQUAL;
    return this->_parse_conf(conf_str, rule_conf);
}

bool    DevFilter::set_filter_inferior(const std::string & conf_str)
{
    RuleConf rule_conf;
    rule_conf.type = INFERIOR;
    return this->_parse_conf(conf_str, rule_conf);
}

bool    DevFilter::set_filter_inferior_equal(const std::string & conf_str)
{
    RuleConf rule_conf;
    rule_conf.type = INFERIOR_EQUAL;
    return this->_parse_conf(conf_str, rule_conf);
}

bool    DevFilter::set_filter_byte_and(const std::string & conf_str)
{
    RuleConf rule_conf;
    rule_conf.type = BYTE_AND;
    return this->_parse_conf(conf_str, rule_conf);
}

bool    DevFilter::set_filter_byte_or(const std::string & conf_str)
{
    RuleConf rule_conf;
    rule_conf.type = BYTE_OR;
    return this->_parse_conf(conf_str, rule_conf);
}

bool    DevFilter::set_filter_byte_xor(const std::string & conf_str)
{
    RuleConf rule_conf;
    rule_conf.type = BYTE_XOR;
    return this->_parse_conf(conf_str, rule_conf);
}

void    DevFilter::_matched(Channel *channel_out, DevFilter::Rule *rule, int64_t out_val)
{
    sihd::util::IArray *array_out = channel_out->array();
    if (rule->notify_if_same
        || memcmp((void *)(array_out->buf_at(rule->write_idx)), (const void *)&out_val, array_out->data_size()) != 0)
    {
        memcpy((void *)(array_out->buf_at(rule->write_idx)),
                (const void *)&out_val,
                array_out->data_size());
        channel_out->notify();
    }
}

void    DevFilter::_apply_rule(sihd::core::Channel *channel, Rule *rule_ptr)
{
    const sihd::util::IArray *array_in = channel->carray();
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
        this->_matched(rule_ptr->channel_out_ptr, rule_ptr, out_val);
    }
}

void    DevFilter::handle(sihd::core::Channel *channel)
{
    auto it = _rules_map.find(channel);
    if (it == _rules_map.end())
        return ;
    for (const std::unique_ptr<Rule> & uptr: it->second)
    {
        this->_apply_rule(channel, uptr.get());
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
        if (this->find_channel(conf.conf_map.at(CONF_KEY_CHANNEL_IN), &channel_in)
                && this->find_channel(conf.conf_map.at(CONF_KEY_CHANNEL_OUT), &channel_out))
        {
            std::unique_ptr<Rule> rule(new Rule());
            if (rule.get()->parse(conf, channel_in, channel_out))
                _rules_map[channel_in].push_back(std::move(rule));
            else
                ret = false;
            ret = ret && this->observe_channel(channel_in);
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

/* ************************************************************************* */
/* DevFilter::Rule */
/* ************************************************************************* */

DevFilter::Rule::Rule():
    channel_in_ptr(nullptr), channel_out_ptr(nullptr),
    write_same_value(true), trigger_idx(0), trigger_value(0),
    write_idx(0), write_value(0),
    notify_if_same(false), should_match(true)
{
}

DevFilter::Rule::~Rule()
{
}

bool    DevFilter::Rule::parse(const DevFilter::RuleConf & conf, Channel *in, Channel *out)
{
    if (in == out)
    {
        SIHD_LOG_ERROR("DevFilter: config error, channel input '%s' and output '%s' are the same",
                        conf.conf_map.at(CONF_KEY_CHANNEL_IN).c_str(), conf.conf_map.at(CONF_KEY_CHANNEL_OUT).c_str());
        return false;
    }
    this->channel_in_ptr = in;
    this->channel_out_ptr = out;
    this->type = conf.type;
    return this->parse_trigger_config(conf)
            && this->parse_write_config(conf)
            && this->parse_options_config(conf)
            && this->verify_parsed();
}

bool    DevFilter::Rule::parse_options_config(const DevFilter::RuleConf & conf)
{
    if (conf.conf_map.find(CONF_KEY_MATCH) != conf.conf_map.end())
    {
        if (sihd::util::Str::convert_from_string<bool>(conf.conf_map.at(CONF_KEY_MATCH), this->should_match) == false)
        {
            SIHD_LOG(error, "DevFilter: conf error for 'not': " << conf.conf_map.at(CONF_KEY_MATCH));
            return false;
        }
    }
    return true;
}

bool    DevFilter::Rule::parse_trigger_config(const DevFilter::RuleConf & conf)
{
    const std::string & conf_trigger = conf.conf_map.at(CONF_KEY_TRIGGER);
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

bool    DevFilter::Rule::parse_write_config(const DevFilter::RuleConf & conf)
{
    if (conf.conf_map.find(CONF_KEY_WRITE) == conf.conf_map.end())
    {
        this->write_idx = this->trigger_idx;
        this->write_same_value = true;
        return true;
    }
    const std::string & conf_write = conf.conf_map.at(CONF_KEY_WRITE);
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

bool    DevFilter::Rule::verify_parsed()
{
    // check if index will be good
    if (this->trigger_idx >= this->channel_in_ptr->array()->size())
    {
        SIHD_LOG_ERROR("DevFilter: trigger index %lu is higher or equal than channel input '%s' size %lu",
                        this->trigger_idx, this->channel_in_ptr->get_name().c_str(), this->channel_in_ptr->array()->size());
        return false;
    }
    if (this->write_idx >= this->channel_out_ptr->array()->size())
    {
        SIHD_LOG_ERROR("DevFilter: write index %lu is higher or equal than channel output '%s' size %lu",
                        this->write_idx, this->channel_out_ptr->get_name().c_str(), this->channel_out_ptr->array()->size());
        return false;
    }
    bool in_array_is_float = this->channel_in_ptr->array()->data_type() == sihd::util::TYPE_FLOAT
                            || this->channel_in_ptr->array()->data_type() == sihd::util::TYPE_DOUBLE;
    // check if trigger value type against channel
    if (this->trigger_value.is_float() && in_array_is_float == false)
    {
        SIHD_LOG_ERROR("DevFilter: type error, trigger value is float and channel input '%s' is not a floating type",
                        this->channel_in_ptr->get_name().c_str());
        return false;
    }
    // check write value type against channel
    bool out_array_is_float = this->channel_out_ptr->array()->data_type() == sihd::util::TYPE_FLOAT
                                || this->channel_out_ptr->array()->data_type() == sihd::util::TYPE_DOUBLE;
    if (out_array_is_float == false
            && ((this->write_same_value && this->trigger_value.is_float())
                || (this->write_same_value == false && this->write_value.is_float())))
    {
        SIHD_LOG_ERROR("DevFilter: type error, write value is float and channel output '%s' is not a floating type",
                        this->channel_out_ptr->get_name().c_str());
        return false;
    }
    return true;
}

}
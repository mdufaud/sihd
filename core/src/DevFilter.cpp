#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/Splitter.hpp>
#include <sihd/util/StrConfiguration.hpp>

#include <sihd/core/DevFilter.hpp>

#define CONF_SETTINGS_DELIMITER ";"
#define CONF_SETTER_DELIMITER "="
#define CONF_INDEX_DELIMITER "="

#define CONF_KEY_CHANNEL_IN "in"
#define CONF_KEY_CHANNEL_OUT "out"
#define CONF_KEY_TRIGGER "trigger"
#define CONF_KEY_WRITE "write"
#define CONF_KEY_MATCH "match"
#define CONF_KEY_DELAY "delay"

namespace sihd::core
{

SIHD_LOGGER;

namespace
{

bool parse_options_config(DevFilter::Rule & rule, const util::StrConfiguration & conf)
{
    auto [key_match, key_delay] = conf.find_all(CONF_KEY_MATCH, CONF_KEY_DELAY);

    if (key_match.has_value())
    {
        if (util::str::convert_from_string<bool>(*key_match, rule.should_match) == false)
        {
            SIHD_LOG_ERROR("DevFilter: conf error for '{}': {}", CONF_KEY_MATCH, *key_match);
            return false;
        }
    }
    if (key_delay.has_value())
    {
        double delay;
        if (util::str::convert_from_string<double>(*key_delay, delay) == false)
        {
            SIHD_LOG_ERROR("DevFilter: conf error for '{}': {}", CONF_KEY_DELAY, *key_delay);
            return false;
        }
        rule.nano_delay = sihd::util::time::from_double(delay);
    }
    return true;
}

bool parse_trigger_config(DevFilter::Rule & rule, const util::StrConfiguration & conf)
{
    auto key_trigger = conf.find(CONF_KEY_TRIGGER);

    sihd::util::Splitter splitter(":");
    splitter.set_empty_delimitations(true);
    std::vector<std::string> split_trigger = splitter.split(*key_trigger);

    // conf -> trigger
    if (split_trigger.size() == 0 || split_trigger.size() > 2)
    {
        SIHD_LOG(error, "DevFilter: trigger conf error: {}", *key_trigger);
        return false;
    }
    if (split_trigger.size() == 1)
    {
        // conf -> trigger=value
        if (split_trigger[0].empty())
        {
            SIHD_LOG(error, "DevFilter: trigger value empty: '{}'", *key_trigger);
            return false;
        }
        rule.trigger_idx = 0;
        rule.trigger_value = util::Value::from_any_string(split_trigger[0]);
        if (rule.trigger_value.empty())
        {
            SIHD_LOG(error, "DevFilter: cannot convert trigger value: {}", split_trigger[0]);
            return false;
        }
    }
    else
    {
        // conf -> trigger=index:value
        if (split_trigger[0].empty() && split_trigger[1].empty())
        {
            SIHD_LOG(error, "DevFilter: trigger idx and value empty: '{}'", *key_trigger);
            return false;
        }
        if (split_trigger[0].empty() == false
            && util::str::convert_from_string<size_t>(split_trigger[0], rule.trigger_idx) == false)
        {
            SIHD_LOG(error, "DevFilter: cannot convert trigger idx: {}", split_trigger[0]);
            return false;
        }
        if (split_trigger[1].empty() == false)
        {
            rule.trigger_value = util::Value::from_any_string(split_trigger[1]);
            if (rule.trigger_value.empty())
            {
                SIHD_LOG(error, "DevFilter: cannot convert trigger value: {}", split_trigger[1]);
                return false;
            }
        }
    }
    return true;
}

bool parse_write_config(DevFilter::Rule & rule, const util::StrConfiguration & conf)
{
    auto key_write = conf.find(CONF_KEY_WRITE);

    if (key_write.has_value() == false)
    {
        rule.write_idx = rule.trigger_idx;
        rule.write_same_value = true;
        return true;
    }

    sihd::util::Splitter splitter(":");
    splitter.set_empty_delimitations(true);
    std::vector<std::string> split_write = splitter.split(*key_write);

    if (split_write.size() == 0 || split_write.size() > 2)
    {
        SIHD_LOG(error, "DevFilter: write conf error: {}", *key_write);
        return false;
    }
    if (split_write.size() == 1)
    {
        // conf -> write=value
        if (split_write[0].empty())
        {
            SIHD_LOG(error, "DevFilter: write value empty: '{}'", *key_write);
            return false;
        }
        rule.write_idx = rule.trigger_idx;
        rule.write_same_value = false;
        rule.write_value = util::Value::from_any_string(split_write[0]);
        if (rule.write_value.empty())
        {
            SIHD_LOG(error, "DevFilter: cannot convert trigger value: {}", split_write[0]);
            return false;
        }
    }
    else
    {
        // conf -> write=index:value
        if (split_write[0].empty() && split_write[1].empty())
        {
            SIHD_LOG(error, "DevFilter: write idx and value empty: '{}'", *key_write);
            return false;
        }
        if (split_write[0].empty() == false
            && util::str::convert_from_string<size_t>(split_write[0], rule.write_idx) == false)
        {
            SIHD_LOG(error, "DevFilter: cannot convert write idx: {}", split_write[0]);
            return false;
        }
        rule.write_same_value = split_write[1].empty();
        if (rule.write_same_value == false)
        {
            rule.write_value = util::Value::from_any_string(split_write[1]);
            if (rule.write_value.empty())
            {
                SIHD_LOG(error, "DevFilter: cannot convert write value: {}", split_write[1]);
                return false;
            }
        }
    }
    return true;
}

} // namespace

SIHD_UTIL_REGISTER_FACTORY(DevFilter)

DevFilter::DevFilter(const std::string & name, sihd::util::Node *parent):
    sihd::core::Device(name, parent),
    _running(false),
    _scheduler_ptr(nullptr),
    _rule_with_delay(false)
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

DevFilter::~DevFilter() = default;

bool DevFilter::_parse_conf(std::string_view rule_str, RuleType type)
{
    // in=channel_path_in;out=channel_path_out;trigger=i:val1;write=j:val2
    Rule rule(type);
    bool ret = rule.parse(rule_str);
    if (ret)
        this->set_filter(rule);
    return ret;
}

void DevFilter::set_filter(const Rule & rule)
{
    if (rule.nano_delay > 0)
        _rule_with_delay = true;
    _rules_lst.push_back(rule);
}

bool DevFilter::set_filter_equal(std::string_view rule_str)
{
    return this->_parse_conf(rule_str, Equal);
}

bool DevFilter::set_filter_superior(std::string_view rule_str)
{
    return this->_parse_conf(rule_str, Superior);
}

bool DevFilter::set_filter_superior_equal(std::string_view rule_str)
{
    return this->_parse_conf(rule_str, SuperiorEqual);
}

bool DevFilter::set_filter_inferior(std::string_view rule_str)
{
    return this->_parse_conf(rule_str, Inferior);
}

bool DevFilter::set_filter_inferior_equal(std::string_view rule_str)
{
    return this->_parse_conf(rule_str, InferiorEqual);
}

bool DevFilter::set_filter_byte_and(std::string_view rule_str)
{
    return this->_parse_conf(rule_str, ByteAnd);
}

bool DevFilter::set_filter_byte_or(std::string_view rule_str)
{
    return this->_parse_conf(rule_str, ByteOr);
}

bool DevFilter::set_filter_byte_xor(std::string_view rule_str)
{
    return this->_parse_conf(rule_str, ByteXor);
}

void DevFilter::_rule_match(Channel *channel_out, const Rule *rule_ptr, int64_t out_val)
{
    const sihd::util::IArray *array_out = channel_out->array();
    channel_out->write({(const int8_t *)&out_val, array_out->data_size()},
                       array_out->byte_index(rule_ptr->write_idx));
}

void DevFilter::_apply_rule(const sihd::core::Channel *channel_in,
                            sihd::core::Channel *channel_out,
                            const Rule *rule_ptr)
{
    const sihd::util::IArray *array_in = channel_in->array();
    sihd::util::Value in_value(array_in->buf_at(rule_ptr->trigger_idx), array_in->data_type());
    bool matched = false;
    switch (rule_ptr->type)
    {
        case Equal:
            matched = in_value == rule_ptr->trigger_value;
            break;
        case Superior:
            matched = in_value > rule_ptr->trigger_value;
            break;
        case SuperiorEqual:
            matched = in_value >= rule_ptr->trigger_value;
            break;
        case Inferior:
            matched = in_value < rule_ptr->trigger_value;
            break;
        case InferiorEqual:
            matched = in_value <= rule_ptr->trigger_value;
            break;
        case ByteAnd:
            matched = in_value.data.n & rule_ptr->trigger_value.data.n;
            break;
        case ByteOr:
            matched = in_value.data.n | rule_ptr->trigger_value.data.n;
            break;
        case ByteXor:
            matched = in_value.data.n ^ rule_ptr->trigger_value.data.n;
            break;
        default:
            break;
    }
    if (rule_ptr->should_match == matched)
    {
        int64_t out_val = rule_ptr->write_same_value ? in_value.data.n : rule_ptr->write_value.data.n;
        if (rule_ptr->nano_delay > 0 && _scheduler_ptr != nullptr)
            _scheduler_ptr->add_task(new DelayWriter(this, channel_out, rule_ptr, out_val));
        else
            this->_rule_match(channel_out, rule_ptr, out_val);
    }
}

void DevFilter::handle(sihd::core::Channel *channel)
{
    auto it = _rules_map.find(channel);
    if (it == _rules_map.end())
        return;
    for (const std::unique_ptr<InternalRule> & rule_uptr : it->second)
    {
        this->_apply_rule(channel, rule_uptr.get()->channel_out_ptr, rule_uptr.get()->rule_ptr);
    }
}

bool DevFilter::is_running() const
{
    return _running;
}

bool DevFilter::on_setup()
{
    return true;
}

bool DevFilter::on_init()
{
    if (_rule_with_delay)
    {
        _scheduler_ptr = this->add_child<sihd::util::Scheduler>(fmt::format("{}-scheduler", this->name()));
        if (_scheduler_ptr != nullptr)
            _scheduler_ptr->set_start_synchronised(true);
        return _scheduler_ptr != nullptr;
    }
    return true;
}

bool DevFilter::on_start()
{
    bool ret;
    Channel *channel_in;
    Channel *channel_out;

    ret = true;
    for (const Rule & conf : _rules_lst)
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
    _running = ret;
    return ret;
}

bool DevFilter::on_stop()
{
    {
        std::lock_guard l(_run_mutex);
        _running = false;
    }
    _rules_map.clear();
    return true;
}

bool DevFilter::on_reset()
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
    trigger_idx(0),
    trigger_value(0),
    write_same_value(true),
    write_idx(0),
    write_value(0),
    should_match(true),
    nano_delay(0)
{
}

DevFilter::Rule::~Rule() = default;

DevFilter::Rule & DevFilter::Rule::in(std::string_view channel_name)
{
    this->channel_in = channel_name;
    return *this;
}

DevFilter::Rule & DevFilter::Rule::out(std::string_view channel_name)
{
    this->channel_out = channel_name;
    return *this;
}

DevFilter::Rule & DevFilter::Rule::match(bool active)
{
    this->should_match = active;
    return *this;
}

DevFilter::Rule & DevFilter::Rule::write_same()
{
    this->write_same_value = true;
    this->write_idx = this->trigger_idx;
    return *this;
}

DevFilter::Rule & DevFilter::Rule::write_same(size_t idx)
{
    this->write_same_value = true;
    this->write_idx = idx;
    return *this;
}

DevFilter::Rule & DevFilter::Rule::delay(double delay)
{
    this->nano_delay = sihd::util::time::from_double(delay);
    return *this;
}

DevFilter::Rule & DevFilter::Rule::delay(time_t nano_delay)
{
    this->nano_delay = nano_delay;
    return *this;
}

bool DevFilter::Rule::parse(std::string_view conf_str)
{
    util::StrConfiguration conf(conf_str);

    auto [channel_in_name, channel_out_name, channel_key_trigger]
        = conf.find_all(CONF_KEY_CHANNEL_IN, CONF_KEY_CHANNEL_OUT, CONF_KEY_TRIGGER);

    if (channel_in_name.has_value() == false)
        SIHD_LOG_ERROR("DevFilter: no channel input '{}' in configuration: {}",
                       CONF_KEY_CHANNEL_IN,
                       conf_str);
    if (channel_out_name.has_value() == false)
        SIHD_LOG_ERROR("DevFilter: no channel output '{}' in configuration: {}",
                       CONF_KEY_CHANNEL_OUT,
                       conf_str);
    if (channel_key_trigger.has_value() == false)
        SIHD_LOG_ERROR("DevFilter: no trigger value '{}' in configuration: {}", CONF_KEY_TRIGGER, conf_str);

    const bool good
        = channel_in_name.has_value() && channel_out_name.has_value() && channel_key_trigger.has_value();
    if (!good)
        return false;

    this->channel_in = *channel_in_name;
    this->channel_out = *channel_out_name;
    return parse_trigger_config(*this, conf) && parse_write_config(*this, conf)
           && parse_options_config(*this, conf);
}

/* ************************************************************************* */
/* DevFilter::DelayWriter */
/* ************************************************************************* */

DevFilter::DelayWriter::DelayWriter(DevFilter *dev,
                                    Channel *channel_out,
                                    const Rule *rule_ptr,
                                    int64_t out_val):
    sihd::util::Task(this, {.run_in = rule_ptr->nano_delay}),
    dev(dev),
    channel_out(channel_out),
    rule_ptr(rule_ptr),
    out_val(out_val)
{
}

DevFilter::DelayWriter::~DelayWriter() = default;

bool DevFilter::DelayWriter::run()
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

DevFilter::InternalRule::~InternalRule() = default;

bool DevFilter::InternalRule::set(const DevFilter::Rule *conf, Channel *in, Channel *out)
{
    if (in == out)
    {
        SIHD_LOG_ERROR("DevFilter: config error, channel input '{}' and output '{}' are the same",
                       conf->channel_in,
                       conf->channel_out);
        return false;
    }
    this->channel_in_ptr = in;
    this->channel_out_ptr = out;
    this->rule_ptr = conf;
    return this->verify();
}

bool DevFilter::InternalRule::verify()
{
    // check if index will be good
    if (rule_ptr->trigger_idx >= this->channel_in_ptr->array()->size())
    {
        SIHD_LOG_ERROR("DevFilter: trigger index %lu is higher or equal than channel input '{}' size %lu",
                       rule_ptr->trigger_idx,
                       rule_ptr->channel_in,
                       this->channel_in_ptr->array()->size());
        return false;
    }
    if (rule_ptr->write_idx >= this->channel_out_ptr->array()->size())
    {
        SIHD_LOG_ERROR("DevFilter: write index %lu is higher or equal than channel output '{}' size %lu",
                       rule_ptr->write_idx,
                       rule_ptr->channel_out,
                       this->channel_out_ptr->array()->size());
        return false;
    }
    bool in_array_is_float = this->channel_in_ptr->array()->data_type() == sihd::util::TYPE_FLOAT
                             || this->channel_in_ptr->array()->data_type() == sihd::util::TYPE_DOUBLE;
    // check if trigger value type against channel
    if (rule_ptr->trigger_value.is_float() && in_array_is_float == false)
    {
        SIHD_LOG_ERROR(
            "DevFilter: type error, trigger value is float and channel input '{}' is not a floating type",
            this->channel_in_ptr->name());
        return false;
    }
    // check write value type against channel
    bool out_array_is_float = this->channel_out_ptr->array()->data_type() == sihd::util::TYPE_FLOAT
                              || this->channel_out_ptr->array()->data_type() == sihd::util::TYPE_DOUBLE;
    if (out_array_is_float == false
        && ((rule_ptr->write_same_value && rule_ptr->trigger_value.is_float())
            || (rule_ptr->write_same_value == false && rule_ptr->write_value.is_float())))
    {
        SIHD_LOG_ERROR(
            "DevFilter: type error, write value is float and channel output '{}' is not a floating type",
            this->channel_out_ptr->name());
        return false;
    }
    return true;
}

} // namespace sihd::core

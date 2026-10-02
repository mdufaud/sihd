#include <sihd/core/DevFilter.hpp>
#include <sihd/sys/NamedFactory.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Splitter.hpp>
#include <sihd/util/StrConfiguration.hpp>

#define CONF_SETTINGS_DELIMITER ";"
#define CONF_SETTER_DELIMITER "="
#define CONF_INDEX_DELIMITER "="

#define CONF_KEY_CHANNEL_IN "in"
#define CONF_KEY_CHANNEL_OUT "out"
#define CONF_KEY_TRIGGER "trigger"
#define CONF_KEY_WRITE "write"
#define CONF_KEY_MATCH "match"
#define CONF_KEY_DELAY "delay"

using enum sihd::util::ErrorCode;

namespace sihd::core
{

SIHD_LOGGER;

namespace
{
std::expected<void, util::Error> parse_options_config(DevFilter::Rule & rule, const util::StrConfiguration & conf)
{
    auto [key_match, key_delay] = conf.find_all(CONF_KEY_MATCH, CONF_KEY_DELAY);

    if (key_match.has_value())
    {
        auto should_match = util::str::convert_from_string<bool>(*key_match);
        SIHD_UNEXPECTED_RETURN(should_match);
        rule.channel_match.invert = *should_match == false;
    }
    if (key_delay.has_value())
    {
        auto delay = util::str::convert_from_string<double>(*key_delay);
        SIHD_UNEXPECTED_RETURN(delay);
        rule.nano_delay = sihd::util::Duration(sihd::util::time::from_double(*delay));
    }
    return {};
}

std::expected<void, util::Error> parse_write_config(DevFilter::Rule & rule, const util::StrConfiguration & conf)
{
    auto key_write = conf.find(CONF_KEY_WRITE);

    if (key_write.has_value() == false)
    {
        rule.write_idx = rule.channel_match.idx;
        rule.write_same_value = true;
        return {};
    }

    sihd::util::Splitter splitter(":");
    splitter.set_empty_delimitations(true);
    std::vector<std::string> split_write = splitter.split(*key_write);

    if (split_write.size() == 0 || split_write.size() > 2)
        return std::unexpected(util::Error(invalid_argument, "write conf error: {}", *key_write));
    if (split_write.size() == 1)
    {
        // conf -> write=value
        if (split_write[0].empty())
            return std::unexpected(util::Error(invalid_argument, "write value empty: '{}'", *key_write));
        rule.write_idx = rule.channel_match.idx;
        rule.write_same_value = false;
        rule.write_value = util::Value::from_any_string(split_write[0]);
        if (rule.write_value.empty())
            return std::unexpected(util::Error(invalid_argument, "cannot convert trigger value: {}", split_write[0]));
    }
    else
    {
        // conf -> write=index:value
        if (split_write[0].empty() && split_write[1].empty())
            return std::unexpected(util::Error(invalid_argument, "write idx and value empty: '{}'", *key_write));
        if (split_write[0].empty() == false)
        {
            auto write_idx = util::str::convert_from_string<size_t>(split_write[0]);
            SIHD_UNEXPECTED_RETURN(write_idx);
            rule.write_idx = *write_idx;
        }
        rule.write_same_value = split_write[1].empty();
        if (rule.write_same_value == false)
        {
            rule.write_value = util::Value::from_any_string(split_write[1]);
            if (rule.write_value.empty())
                return std::unexpected(util::Error(invalid_argument, "cannot convert write value: {}", split_write[1]));
        }
    }
    return {};
}

} // namespace

SIHD_REGISTER_FACTORY(DevFilter)

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

std::expected<void, sihd::util::Error> DevFilter::_parse_conf(std::string_view rule_str,
                                                              ChannelMatch::Comparison comparison)
{
    // in=channel_path_in;out=channel_path_out;trigger=i:val1;write=j:val2
    Rule rule(comparison);
    auto res = rule.parse(rule_str);
    if (res.has_value() == false)
        return res;
    this->set_filter(rule);
    return {};
}

bool DevFilter::_set_filter_conf(std::string_view rule_str, ChannelMatch::Comparison comparison)
{
    const auto res = this->_parse_conf(rule_str, comparison);
    return !SIHD_UNEXPECTED_LOG(res);
}

void DevFilter::set_filter(const Rule & rule)
{
    if (rule.nano_delay > 0)
        _rule_with_delay = true;
    _rules_lst.push_back(rule);
}

bool DevFilter::set_filter_equal(std::string_view rule_str)
{
    return this->_set_filter_conf(rule_str, ChannelMatch::Equal);
}

bool DevFilter::set_filter_superior(std::string_view rule_str)
{
    return this->_set_filter_conf(rule_str, ChannelMatch::Superior);
}

bool DevFilter::set_filter_superior_equal(std::string_view rule_str)
{
    return this->_set_filter_conf(rule_str, ChannelMatch::SuperiorEqual);
}

bool DevFilter::set_filter_inferior(std::string_view rule_str)
{
    return this->_set_filter_conf(rule_str, ChannelMatch::Inferior);
}

bool DevFilter::set_filter_inferior_equal(std::string_view rule_str)
{
    return this->_set_filter_conf(rule_str, ChannelMatch::InferiorEqual);
}

bool DevFilter::set_filter_byte_and(std::string_view rule_str)
{
    return this->_set_filter_conf(rule_str, ChannelMatch::ByteAnd);
}

bool DevFilter::set_filter_byte_or(std::string_view rule_str)
{
    return this->_set_filter_conf(rule_str, ChannelMatch::ByteOr);
}

bool DevFilter::set_filter_byte_xor(std::string_view rule_str)
{
    return this->_set_filter_conf(rule_str, ChannelMatch::ByteXor);
}

void DevFilter::_rule_match(Channel *channel_out, const Rule *rule_ptr, int64_t out_val)
{
    const sihd::util::IArray *array_out = channel_out->array();
    channel_out->write({(const int8_t *)&out_val, array_out->data_size()}, array_out->byte_index(rule_ptr->write_idx));
}

void DevFilter::_apply_rule(const sihd::core::Channel *channel_in,
                            sihd::core::Channel *channel_out,
                            const Rule *rule_ptr)
{
    const sihd::util::Value in_value = channel_in->value_at(rule_ptr->channel_match.idx);
    if (in_value.empty() || rule_ptr->channel_match.match(in_value) == false)
        return;
    const int64_t out_val = rule_ptr->write_same_value ? in_value.data.n : rule_ptr->write_value.data.n;
    if (rule_ptr->nano_delay > 0 && _scheduler_ptr != nullptr)
        _scheduler_ptr->add_task(new DelayWriter(this, channel_out, rule_ptr, out_val));
    else
        this->_rule_match(channel_out, rule_ptr, out_val);
}

void DevFilter::handle(sihd::core::Channel *channel)
{
    std::lock_guard l(_run_mutex);
    if (_running == false)
        return;
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
    for (const Rule & conf : _rules_lst)
    {
        auto channel_in = this->find_channel(conf.channel_in);
        if (SIHD_UNEXPECTED_LOG(channel_in))
            return false;
        auto channel_out = this->find_channel(conf.channel_out);
        if (SIHD_UNEXPECTED_LOG(channel_out))
            return false;
        std::unique_ptr<InternalRule> rule(new InternalRule());
        auto res = rule->set(&conf, *channel_in, *channel_out);
        if (!res)
        {
            SIHD_LOG(error, "{}", res.error().message);
            return false;
        }
        if (this->observe_channel(*channel_in) == false)
        {
            SIHD_LOG(error, "cannot observe channel '{}'", conf.channel_in);
            return false;
        }
        _rules_map[*channel_in].push_back(std::move(rule));
    }

    std::lock_guard l(_run_mutex);
    _running = true;
    return true;
}

bool DevFilter::on_stop()
{
    {
        std::lock_guard l(_run_mutex);
        _running = false;
        _rules_map.clear();
    }
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

DevFilter::Rule::Rule(ChannelMatch::Comparison comparison):
    channel_match(comparison, 0),
    write_same_value(true),
    write_idx(0),
    write_value(0),
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
    this->channel_match.invert = active == false;
    return *this;
}

DevFilter::Rule & DevFilter::Rule::write_same()
{
    this->write_same_value = true;
    this->write_idx = this->channel_match.idx;
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
    this->nano_delay = sihd::util::Duration(sihd::util::time::from_double(delay));
    return *this;
}

DevFilter::Rule & DevFilter::Rule::delay(sihd::util::Duration nano_delay)
{
    this->nano_delay = nano_delay;
    return *this;
}

std::expected<void, sihd::util::Error> DevFilter::Rule::parse(std::string_view conf_str)
{
    util::StrConfiguration conf(conf_str);

    auto [channel_in_name, channel_out_name, channel_key_trigger] = conf.find_all(CONF_KEY_CHANNEL_IN,
                                                                                  CONF_KEY_CHANNEL_OUT,
                                                                                  CONF_KEY_TRIGGER);

    if (channel_in_name.has_value() == false)
        return std::unexpected(
            util::Error(not_found, "no channel input '{}' in configuration: {}", CONF_KEY_CHANNEL_IN, conf_str));
    if (channel_out_name.has_value() == false)
        return std::unexpected(
            util::Error(not_found, "no channel output '{}' in configuration: {}", CONF_KEY_CHANNEL_OUT, conf_str));
    if (channel_key_trigger.has_value() == false)
        return std::unexpected(
            util::Error(not_found, "no trigger value '{}' in configuration: {}", CONF_KEY_TRIGGER, conf_str));

    this->channel_in = *channel_in_name;
    this->channel_out = *channel_out_name;
    auto trigger = this->channel_match.parse_trigger(*channel_key_trigger);
    SIHD_UNEXPECTED_RETURN(trigger);
    auto res = parse_write_config(*this, conf);
    if (res.has_value() == false)
        return res;
    return parse_options_config(*this, conf);
}

/* ************************************************************************* */
/* DevFilter::DelayWriter */
/* ************************************************************************* */

DevFilter::DelayWriter::DelayWriter(DevFilter *dev, Channel *channel_out, const Rule *rule_ptr, int64_t out_val):
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

DevFilter::InternalRule::InternalRule(): channel_in_ptr(nullptr), channel_out_ptr(nullptr), rule_ptr(nullptr) {}

DevFilter::InternalRule::~InternalRule() = default;

std::expected<void, sihd::util::Error>
    DevFilter::InternalRule::set(const DevFilter::Rule *conf, Channel *in, Channel *out)
{
    if (in == out)
        return std::unexpected(util::Error(invalid_argument,
                                           "config error, channel input '{}' and output '{}' are the same",
                                           conf->channel_in,
                                           conf->channel_out));
    this->channel_in_ptr = in;
    this->channel_out_ptr = out;
    this->rule_ptr = conf;
    return this->verify();
}

std::expected<void, sihd::util::Error> DevFilter::InternalRule::verify()
{
    if (rule_ptr->channel_match.verify(this->channel_in_ptr) == false)
        return std::unexpected(util::Error(invalid_argument,
                                           "invalid trigger index {} for channel input '{}'",
                                           rule_ptr->channel_match.idx,
                                           rule_ptr->channel_in));
    if (rule_ptr->write_idx >= this->channel_out_ptr->array()->size())
        return std::unexpected(util::Error(invalid_argument,
                                           "write index {} is higher or equal than channel output '{}' size {}",
                                           rule_ptr->write_idx,
                                           rule_ptr->channel_out,
                                           this->channel_out_ptr->array()->size()));
    const bool out_array_is_float = this->channel_out_ptr->array()->data_type() == sihd::util::TYPE_FLOAT
                                    || this->channel_out_ptr->array()->data_type() == sihd::util::TYPE_DOUBLE;
    if (out_array_is_float == false
        && ((rule_ptr->write_same_value && rule_ptr->channel_match.value.is_float())
            || (rule_ptr->write_same_value == false && rule_ptr->write_value.is_float())))
        return std::unexpected(
            util::Error(invalid_argument,
                        "type error, write value is float and channel output '{}' is not a floating type",
                        this->channel_out_ptr->name()));
    return {};
}

} // namespace sihd::core

#include <sihd/core/ChannelMatch.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Splitter.hpp>
#include <sihd/util/StrConfiguration.hpp>
#include <sihd/util/str.hpp>

namespace sihd::core
{

using enum sihd::util::ErrorCode;
using namespace sihd::util;

SIHD_LOGGER;

namespace
{

std::expected<void, Error> convert_trigger_value(const std::string & str, Value & value)
{
    value = Value::from_any_string(str);
    if (value.empty())
        return std::unexpected(Error(invalid_argument, "cannot convert trigger value '{}'", str));
    return {};
}

} // namespace

ChannelMatch::ChannelMatch(Comparison comparison, Value value, size_t idx):
    comparison(comparison),
    idx(idx),
    value(value)
{
}

const char *ChannelMatch::comparison_str(Comparison comparison)
{
    switch (comparison)
    {
        case Equal:
            return "equal";
        case Superior:
            return "superior";
        case SuperiorEqual:
            return "superior_equal";
        case Inferior:
            return "inferior";
        case InferiorEqual:
            return "inferior_equal";
        case ByteAnd:
            return "byte_and";
        case ByteOr:
            return "byte_or";
        case ByteXor:
            return "byte_xor";
        default:
            return "none";
    }
}

ChannelMatch::Comparison ChannelMatch::comparison_from_str(std::string_view str)
{
    for (Comparison comparison : {Equal, Superior, SuperiorEqual, Inferior, InferiorEqual, ByteAnd, ByteOr, ByteXor})
    {
        if (str == comparison_str(comparison))
            return comparison;
    }
    return None;
}

std::expected<void, Error> ChannelMatch::parse(std::string_view conf_str)
{
    StrConfiguration conf(conf_str);
    auto [key_cmp, key_idx, key_value, key_invert] = conf.find_all("cmp", "idx", "value", "invert");

    if (key_cmp.has_value() == false)
        return std::unexpected(Error(not_found, "no comparison 'cmp' in configuration: '{}'", conf_str));
    const Comparison cmp = this->comparison_from_str(*key_cmp);
    if (cmp == None)
        return std::unexpected(
            Error(invalid_argument, "unknown comparison '{}' in configuration: '{}'", *key_cmp, conf_str));
    size_t parsed_idx = this->idx;
    Value parsed_value = this->value;
    bool parsed_invert = this->invert;
    if (key_idx.has_value())
    {
        const auto idx = str::convert_from_string<size_t>(*key_idx);
        SIHD_UNEXPECTED_RETURN(idx);
        parsed_idx = *idx;
    }
    if (key_value.has_value())
    {
        parsed_value = Value::from_any_string(*key_value);
        if (parsed_value.empty())
            return std::unexpected(Error(invalid_argument, "cannot convert value: {}", *key_value));
    }
    if (key_invert.has_value())
    {
        const auto invert = str::convert_from_string<bool>(*key_invert);
        SIHD_UNEXPECTED_RETURN(invert);
        parsed_invert = *invert;
    }
    this->comparison = cmp;
    this->idx = parsed_idx;
    this->value = parsed_value;
    this->invert = parsed_invert;
    return {};
}

std::expected<void, Error> ChannelMatch::parse_trigger(std::string_view conf)
{
    Splitter splitter(":");
    splitter.set_empty_delimitations(true);
    const std::vector<std::string> split = splitter.split(conf);

    if (split.empty() || split.size() > 2)
        return std::unexpected(Error(invalid_argument, "trigger conf error: '{}'", conf));
    if (split.size() == 1)
    {
        // trigger=value
        if (split[0].empty())
            return std::unexpected(Error(invalid_argument, "trigger value empty: '{}'", conf));
        this->idx = 0;
        return convert_trigger_value(split[0], this->value);
    }
    // trigger=index:value
    if (split[0].empty() && split[1].empty())
        return std::unexpected(Error(invalid_argument, "trigger idx and value empty: '{}'", conf));
    if (split[0].empty() == false)
    {
        const auto idx = str::convert_from_string<size_t>(split[0]);
        SIHD_UNEXPECTED_RETURN(idx);
        this->idx = *idx;
    }
    if (split[1].empty() == false)
        return convert_trigger_value(split[1], this->value);
    return {};
}

bool ChannelMatch::match(const Channel *channel) const
{
    if (channel == nullptr)
        return false;
    const Value in_value = channel->value_at(this->idx);
    if (in_value.empty())
        return false;
    return this->match(in_value);
}

bool ChannelMatch::match(const Value & in_value) const
{
    if (in_value.empty() || this->comparison == None)
        return false;
    bool matched = false;
    switch (this->comparison)
    {
        case Equal:
            matched = in_value == this->value;
            break;
        case Superior:
            matched = in_value > this->value;
            break;
        case SuperiorEqual:
            matched = in_value >= this->value;
            break;
        case Inferior:
            matched = in_value < this->value;
            break;
        case InferiorEqual:
            matched = in_value <= this->value;
            break;
        case ByteAnd:
            matched = (in_value.data.n & this->value.data.n) != 0;
            break;
        case ByteOr:
            matched = (in_value.data.n | this->value.data.n) != 0;
            break;
        case ByteXor:
            matched = (in_value.data.n ^ this->value.data.n) != 0;
            break;
        default:
            break;
    }
    return matched != this->invert;
}

bool ChannelMatch::verify(const Channel *channel) const
{
    if (channel == nullptr)
        return false;
    if (this->comparison == None)
    {
        SIHD_LOG(error, "ChannelMatch: no comparison set");
        return false;
    }
    const IArray *array = channel->array();
    if (this->idx >= array->size())
    {
        SIHD_LOG(error,
                 "ChannelMatch: index {} is out of range of channel '{}' size {}",
                 this->idx,
                 channel->full_name(),
                 array->size());
        return false;
    }
    const bool array_is_float = array->data_type() == TYPE_FLOAT || array->data_type() == TYPE_DOUBLE;
    if (this->value.is_float() && array_is_float == false)
    {
        SIHD_LOG(error, "ChannelMatch: value is float and channel '{}' is not a floating type", channel->full_name());
        return false;
    }
    return true;
}

} // namespace sihd::core

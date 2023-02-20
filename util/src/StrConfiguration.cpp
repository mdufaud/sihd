#include <fmt/format.h>

#include <sihd/util/Splitter.hpp>
#include <sihd/util/StrConfiguration.hpp>

namespace sihd::util
{

StrConfiguration::StrConfiguration(std::string_view conf)
{
    this->parse_configuration(conf);
}

void StrConfiguration::parse_configuration(std::string_view conf)
{
    _configuration.clear();
    Splitter splitter(";");
    auto split_pairs = splitter.split(conf);
    for (const auto & pair : split_pairs)
    {
        size_t idx = pair.find_first_of('=');
        if (idx != std::string::npos)
        {
            std::string key = pair.substr(0, idx);
            std::string value = pair.substr(idx + 1, pair.size());
            _configuration[key] = value;
        }
    }
}

size_t StrConfiguration::size() const
{
    return _configuration.size();
}

bool StrConfiguration::has(const std::string & key) const
{
    return _configuration.count(key) > 0;
}

std::optional<std::string> StrConfiguration::find(const std::string & key) const
{
    const auto it = _configuration.find(key);
    if (it == _configuration.end())
        return std::nullopt;
    return it->second;
}

std::string StrConfiguration::operator[](const std::string & key) const
{
    return this->find(key).value_or("");
}

std::string StrConfiguration::get(const std::string & key) const
{
    const auto found = this->find(key);
    if (found.has_value() == false)
        throw std::out_of_range(fmt::format("StrConfiguration::get key not found: '{}'", key));
    return *found;
}

std::string StrConfiguration::str() const
{
    bool first = true;
    std::string ret;

    for (const auto & [key, value] : _configuration)
    {
        ret += fmt::format("{}{}={}", (first ? ";" : ""), key, value);
        first = false;
    }

    return ret;
}

} // namespace sihd::util
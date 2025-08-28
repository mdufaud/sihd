#include <fmt/format.h>

#include <sihd/util/Splitter.hpp>
#include <sihd/util/StrConfiguration.hpp>
#include <sihd/util/str.hpp>

namespace sihd::util
{

StrConfiguration::StrConfiguration(std::string_view conf)
{
    this->parse_configuration(conf);
}

void StrConfiguration::parse_configuration(std::string_view conf)
{
    _configuration.clear();

    if (conf.empty())
        return;

    Splitter splitter(";");
    std::vector<std::string> conf_list = splitter.split(conf);
    for (const std::string & conf : conf_list)
    {
        auto [key, value] = str::split_pair(conf, "=");
        if (!key.empty())
        {
            _configuration.emplace(std::move(key), std::move(value));
        }
    }
}

bool StrConfiguration::empty() const
{
    return _configuration.empty();
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

const std::string & StrConfiguration::get(const std::string & key) const
{
    const auto it = _configuration.find(key);
    if (it == _configuration.end())
        throw std::out_of_range(fmt::format("StrConfiguration::get key not found: '{}'", key));
    return it->second;
}

std::string StrConfiguration::str() const
{
    std::string ret;
    bool first = true;

    for (const auto & [key, value] : _configuration)
    {
        ret += fmt::format("{}{}={}", (first ? ";" : ""), key, value);
        first = false;
    }

    return ret;
}

} // namespace sihd::util
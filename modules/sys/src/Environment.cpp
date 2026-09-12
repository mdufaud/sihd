#include "internal/environment.hpp"

#include <utility>

#include <sihd/sys/Environment.hpp>
#include <sihd/sys/env.hpp>
#include <sihd/util/str.hpp>

namespace sihd::sys
{

using namespace sihd::util;

namespace
{

template <typename Range>
void load_entries(const Range & range, std::map<std::string, std::string> & entries)
{
    for (const auto & entry : range)
    {
        const auto [key, value] = str::split_pair_view(entry, "=");
        if (!key.empty())
            entries.insert_or_assign(std::string(key), std::string(value));
    }
}

} // namespace

Environment Environment::from_current()
{
    Environment env;
    env._entries = env::list();
    return env;
}

void Environment::load(std::span<std::string_view> entries)
{
    load_entries(entries, _entries);
}

void Environment::load(std::span<const std::string> entries)
{
    load_entries(entries, _entries);
}

void Environment::load(std::span<const char *> entries)
{
    load_entries(entries, _entries);
}

void Environment::load(std::initializer_list<std::string_view> entries)
{
    load_entries(entries, _entries);
}

void Environment::set(std::string_view key, std::string_view value)
{
    _entries.insert_or_assign(std::string(key), std::string(value));
}

std::optional<std::string> Environment::get(std::string_view key) const
{
    const auto it = _entries.find(std::string(key));
    if (it == _entries.end())
        return std::nullopt;
    return it->second;
}

bool Environment::rm(std::string_view key)
{
    return _entries.erase(std::string(key)) > 0;
}

void Environment::clear()
{
    _entries.clear();
}

bool Environment::empty() const
{
    return _entries.empty();
}

size_t Environment::size() const
{
    return _entries.size();
}

const std::map<std::string, std::string> & Environment::entries() const
{
    return _entries;
}

std::string Environment::expand(std::string_view str) const
{
    return internal::expand_entries(str, [this](std::string_view key) { return this->get(key); });
}

} // namespace sihd::sys

namespace sihd::sys::internal
{

namespace
{

bool is_name_char(char c)
{
    return str::is_digit(c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

} // namespace

std::string expand_entries(std::string_view str, const EntryLookup & lookup)
{
    std::string out;
    out.reserve(str.size());

    size_t pos = 0;
    while (pos < str.size())
    {
        const size_t dollar = str.find('$', pos);
        if (dollar == std::string_view::npos)
        {
            out.append(str.substr(pos));
            break;
        }
        out.append(str.substr(pos, dollar - pos));

        std::string_view name;
        size_t ref_end;
        if (dollar + 1 < str.size() && str[dollar + 1] == '{')
        {
            const size_t closing = str.find('}', dollar + 2);
            if (closing == std::string_view::npos)
            {
                // unterminated reference: keep the rest as-is
                out.append(str.substr(dollar));
                break;
            }
            name = str.substr(dollar + 2, closing - dollar - 2);
            ref_end = closing + 1;
        }
        else
        {
            size_t name_end = dollar + 1;
            while (name_end < str.size() && is_name_char(str[name_end]))
                ++name_end;
            name = str.substr(dollar + 1, name_end - dollar - 1);
            ref_end = name_end;
        }

        const std::optional<std::string> value = name.empty() ? std::nullopt : lookup(name);
        if (value.has_value())
            out.append(*value);
        else
            out.append(str.substr(dollar, ref_end - dollar));
        pos = ref_end;
    }
    return out;
}

} // namespace sihd::sys::internal

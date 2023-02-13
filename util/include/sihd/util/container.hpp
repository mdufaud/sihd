#ifndef __SIHD_UTIL_CONTAINER_HPP__
# define __SIHD_UTIL_CONTAINER_HPP__

# include <fmt/format.h>

# include <vector>

# include <sihd/util/traits.hpp>

namespace sihd::util::container
{

template <typename Container, typename Value>
bool contains(const Container & container, const Value & value)
{
    return std::find(container.begin(), container.end(), value) != container.end();
}

template <typename Container, typename Value>
bool erase(Container & container, const Value & value)
{
    static_assert(traits::IsIterable<Container>::value, "Type must be iterable");

    auto it = std::remove(container.begin(), container.end(), value);
    bool ret = it != container.end();
    container.erase(it, container.end());
    return ret;
}

template <typename T>
bool emplace_unique(std::vector<T> & vec, const T & value)
{
    const auto it = std::find(vec.begin(), vec.end(), value);
    const bool ret = it == vec.end();
    if (ret)
        vec.emplace_back(value);
    return ret;
}

template <typename Container, typename Key, typename Value>
Value get_or(const Container & container, const Key & key, const Value & default_value)
{
    static_assert(traits::IsIterable<Container>::value, "Type must be iterable");
    static_assert(std::is_convertible_v<typename Container::mapped_type, Value>, "Type mismatch in get_or");

    const auto it = container.find(key);
    return it != container.end() ? it->second : default_value;
}

template <typename Container, typename Key, typename... Keys>
auto *recursive_search(Container & container, const Key & key, const Keys & ...keys)
{
    const auto it = container.find(key);
    const auto found = it != container.end();

    if constexpr (sizeof...(Keys) == 0)
        return found ? &(it->second) : nullptr;
    else
        return found ? recursive_search(it->second, keys...) : nullptr;
}

template <typename Container, typename Key, typename... Keys>
auto & recursive_get(Container & container, const Key & key, const Keys & ...keys)
{
    if constexpr (std::is_const_v<Container>)
    {
        const auto & ref = container.at(key);
        if constexpr (sizeof...(Keys) == 0)
            return ref;
        else
            return recursive_get(ref, keys...);
    }
    else
    {
        auto & ref = container[key];
        if constexpr (sizeof...(Keys) == 0)
            return ref;
        else
            return recursive_get(ref, keys...);
    }
}

template <typename Map, typename Predicate>
std::vector<typename Map::key_type> ordered_keys(const Map & map, Predicate && pred)
{
    std::vector<typename Map::key_type> ret;
    ret.reserve(map.size());

    for (const auto & [key, _]: map)
    {
        ret.emplace_back(key);
    }

    std::sort(ret.begin(), ret.end(), pred);
    return ret;
}

template <typename Map>
std::vector<typename Map::key_type> ordered_keys(const Map & map)
{
    return ordered_keys(map, std::less<typename Map::key_type>());
}

template <typename T, traits::disable_if_map<T> = 0>
std::string str(const T & container)
{
    static_assert(traits::IsIterable<T>::value);
    std::string s = "[";
    for (auto it = container.begin(), first = it; it != container.end(); ++it)
    {
        s += fmt::format("{}{}", it != first ? ", " : "", *it);
    }
    s += "]";
    return s;
}

template <typename T, traits::enable_if_map<T> = 0>
std::string str(const T & container)
{
    std::string s = "{";
    for (auto it = container.begin(), first = it; it != container.end(); ++it)
    {
        s += fmt::format("{}{}: {}", it != first ? ", " : "", it->first, it->second);
    }
    s += "}";
    return s;
}

}

#endif
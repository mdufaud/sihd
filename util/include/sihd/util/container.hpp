#ifndef __SIHD_UTIL_CONTAINER_HPP__
#define __SIHD_UTIL_CONTAINER_HPP__

#include <algorithm>
#include <numeric>
#include <tuple>
#include <vector>

#include <sihd/util/traits.hpp>

namespace sihd::util::container
{

namespace details
{

template <typename Predicate, typename Container>
using predicate_return_key_type = typename std::invoke_result_t<Predicate, typename Container::key_type>;

template <typename Predicate, typename Container>
using predicate_return_value_type = typename std::invoke_result_t<Predicate, typename Container::value_type>;

template <typename T, int, int, typename SFINAE = void>
struct recursive_map_type
{
        using type = T;
};

template <typename T, int CurrentKey, int TotalKeys>
struct recursive_map_type<T,
                          CurrentKey,
                          TotalKeys,
                          std::enable_if_t<(CurrentKey < TotalKeys) && traits::is_map<T>::value>>
{
        using map_value_type = typename T::mapped_type;

        // if mapped type is a map, do a recursion else get type
        using type = std::conditional_t<traits::is_map<map_value_type>::value,
                                        typename recursive_map_type<map_value_type, CurrentKey + 1, TotalKeys>::type,
                                        map_value_type>;
};

template <typename T, typename... Keys>
struct recursive_map_type_helper
{
    private:
        using return_type = typename recursive_map_type<T, 0, sizeof...(Keys) + 1>::type;

    public:
        // handle const type
        using type = std::conditional_t<std::is_const_v<T>, const return_type, return_type>;
};

static constexpr auto return_same_predicate = [](const auto & v) {
    return v;
};

} // namespace details

template <typename Container, typename Value>
auto find(Container & container, const Value & value)
{
    return std::find(container.begin(), container.end(), value);
}

template <typename Container, typename Predicate>
auto find_if(Container & container, const Predicate & predicate)
{
    return std::find_if(container.begin(), container.end(), predicate);
}

template <typename Container, typename Value>
bool contains(const Container & container, const Value & value)
{
    return std::find(container.begin(), container.end(), value) != container.end();
}

template <typename Container, typename Predicate>
void sort(Container & container, const Predicate & predicate)
{
    std::sort(container.begin(), container.end(), predicate);
}

template <typename Container, typename Value>
bool erase(Container & container, const Value & value)
{
    static_assert(traits::is_iterable<Container>::value, "Type must be iterable");

    auto it = std::remove(container.begin(), container.end(), value);
    bool ret = it != container.end();
    container.erase(it, container.end());
    return ret;
}

template <typename Container, typename Predicate>
bool erase_if(Container & container, const Predicate & predicate)
{
    static_assert(traits::is_iterable<Container>::value, "Type must be iterable");

    auto it = std::remove_if(container.begin(), container.end(), predicate);
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

template <typename Container, typename T>
bool emplace_back_unique(Container & container, const T & value)
{
    const auto it = std::find(container.begin(), container.end(), value);
    const bool ret = it == container.end();
    if (ret)
        container.emplace_back(value);
    return ret;
}

template <typename Container, typename T>
bool emplace_front_unique(Container & container, const T & value)
{
    const auto it = std::find(container.begin(), container.end(), value);
    const bool ret = it == container.end();
    if (ret)
        container.emplace_front(value);
    return ret;
}

template <typename Container, typename Key, typename Value>
Value get_or(const Container & container, const Key & key, const Value & default_value)
{
    static_assert(traits::is_iterable<Container>::value, "Type must be iterable");
    static_assert(std::is_convertible_v<typename Container::mapped_type, Value>, "Type mismatch in get_or");

    const auto it = container.find(key);
    return it != container.end() ? it->second : default_value;
}

template <typename Container,
          typename Key,
          typename... Keys,
          typename Type = typename details::recursive_map_type_helper<Container, Keys...>::type,
          typename std::enable_if_t<traits::is_map<Container>::value, bool> = 0>
Type *recursive_map_search(Container & container, const Key & key, const Keys &...keys)
{
    const auto it = container.find(key);
    const bool found = it != container.end();

    if constexpr (sizeof...(Keys) == 0)
        return found ? &(it->second) : nullptr;
    else
        return found ? recursive_map_search(it->second, keys...) : nullptr;
}

template <typename Container, typename Key, typename... Keys>
auto & recursive_get(Container & container, const Key & key, const Keys &...keys)
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

template <typename Container,
          typename Predicate = decltype(std::less<typename Container::key_type>()),
          typename KeyType = typename Container::key_type,
          typename std::enable_if_t<traits::is_map<Container>::value, bool> = 0>
std::vector<KeyType> ordered_keys(const Container & map, const Predicate & pred = std::less<KeyType>())
{
    std::vector<KeyType> ret;

    ret.reserve(map.size());
    for (const auto & [key, _] : map)
    {
        ret.emplace_back(key);
    }
    std::sort(ret.begin(), ret.end(), pred);
    return ret;
}

template <typename ContainerTo,
          typename ContainerFrom,
          typename std::enable_if_t<traits::is_iterable<ContainerTo>::value, bool> = 0,
          typename std::enable_if_t<traits::is_iterable<ContainerFrom>::value, bool> = 0>
void insert_at_end(ContainerTo & to, ContainerFrom && from)
{
    if constexpr (std::is_rvalue_reference_v<decltype(from)>)
        to.insert(to.end(), std::make_move_iterator(from.begin()), std::make_move_iterator(from.end()));
    else
        to.insert(to.end(), from.begin(), from.end());
}

template <typename Container,
          typename Predicate = decltype(details::return_same_predicate),
          typename Type = typename details::predicate_return_value_type<Predicate, Container>,
          typename std::enable_if_t<traits::is_iterable<Container>::value, bool> = 0>
Type sum(const Container & container, const Predicate & predicate = details::return_same_predicate)
{
    return std::accumulate(
        container.begin(),
        container.end(),
        Type {},
        [&predicate](auto accumulated, const auto & val) { return std::move(accumulated) + predicate(val); });
}

template <typename Container,
          typename Predicate = decltype(details::return_same_predicate),
          typename std::enable_if_t<traits::is_iterable<Container>::value, bool> = 0>
double average(const Container & container, const Predicate & predicate = details::return_same_predicate)
{
    if (container.begin() == container.end())
        return 0.;

    return static_cast<double>(sum(container, predicate)) / std::distance(container.begin(), container.end());
}

template <int... Columns, typename Tuple>
void sort_tuple(std::vector<Tuple> & tuples)
{
    std::sort(tuples.begin(), tuples.end(), [](const auto & lhs, const auto & rhs) {
        return std::make_tuple(std::get<Columns>(lhs)...) < std::make_tuple(std::get<Columns>(rhs)...);
    });
}

template <typename Container,
          typename Predicate,
          typename Transform,
          typename ReturnType = typename details::predicate_return_value_type<Transform, Container>>
std::vector<ReturnType> transform_if(const Container & container, Predicate && pred, Transform && transform)
{
    std::vector<ReturnType> output;

    for (const auto & elem : container)
    {
        if (pred(elem))
        {
            output.push_back(transform(elem));
        }
    }

    return output;
}

} // namespace sihd::util::container

#endif

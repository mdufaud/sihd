#ifndef __SIHD_UTIL_TRAITS_HPP__
# define __SIHD_UTIL_TRAITS_HPP__

# include <type_traits>

namespace sihd::util::traits
{

template <typename, typename = void>
struct is_iterable: public std::false_type {};

template <typename T>
struct is_iterable<T, std::void_t<decltype(std::declval<T>().begin()),
                                    decltype(std::declval<T>().end())>>: public std::true_type {};

template <typename T>
using enable_if_iterable = typename std::enable_if_t<is_iterable<T>::value, bool>;

template <typename T>
using disable_if_iterable = typename std::enable_if_t<!is_iterable<T>::value, bool>;

template <typename, typename = void>
struct is_map: public std::false_type {};

template <typename T>
struct is_map<T, std::void_t<typename T::mapped_type>>: public std::true_type {};

template <typename T>
using enable_if_map = typename std::enable_if_t<is_map<T>::value, bool>;

template <typename T>
using disable_if_map = typename std::enable_if_t<!is_map<T>::value, bool>;

}

#endif
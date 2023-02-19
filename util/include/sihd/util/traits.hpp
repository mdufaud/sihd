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

template <typename, typename = void>
struct has_data_size: public std::false_type {};

template <typename T>
struct has_data_size<T, std::void_t<decltype(std::declval<T>().data()),
                                    decltype(std::declval<T>().size())>>: public std::true_type {};

template <typename, typename = void>
struct is_map: public std::false_type {};

template <typename T>
struct is_map<T, std::void_t<typename T::mapped_type>>: public std::true_type {};

}

#endif
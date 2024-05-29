#ifndef __SIHD_UTIL_TRAITS_HPP__
#define __SIHD_UTIL_TRAITS_HPP__

#include <type_traits>

namespace sihd::util::traits
{

template <typename, typename = void>
struct is_iterable: public std::false_type
{
};

template <typename T>
struct is_iterable<T, std::void_t<decltype(std::declval<T>().begin()), decltype(std::declval<T>().end())>>
    : public std::true_type
{
};

template <typename, typename = void>
struct has_data_size: public std::false_type
{
};

template <typename T>
struct has_data_size<T, std::void_t<decltype(std::declval<T>().data()), decltype(std::declval<T>().size())>>
    : public std::true_type
{
};

template <typename, typename = void>
struct is_map: public std::false_type
{
};

template <typename T>
struct is_map<T, std::void_t<typename T::mapped_type>>: public std::true_type
{
};

template <typename, typename = void>
struct is_duration: public std::false_type
{
};

template <typename T>
struct is_duration<T, std::void_t<typename T::period>>: public std::true_type
{
};

template <typename, typename = void>
struct is_std_function: public std::false_type
{
};

template <typename T>
struct is_std_function<T, std::void_t<typename T::result_type>>: public std::true_type
{
};

template <class T, class U>
using is_same_uncvref
    = std::is_same<std::remove_cv_t<std::remove_reference_t<T>>, std::remove_cv_t<std::remove_reference_t<U>>>;

template <typename T, typename... U>
using are_all_same = std::integral_constant<bool, (... && std::is_same_v<T, U>)>;

template <typename T, typename... U>
using are_all_same_decay = std::integral_constant<bool, (... && std::is_same_v<T, std::decay_t<U>>)>;

template <typename To, typename... From>
using are_all_convertible = std::integral_constant<bool, (... && std::is_convertible_v<From, To>)>;

template <typename T, typename... Args>
using are_all_constructible = std::integral_constant<bool, (... && std::is_constructible_v<T, Args>)>;

} // namespace sihd::util::traits

#endif
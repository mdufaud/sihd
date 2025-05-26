#ifndef __SIHD_UTIL_TRAITS_HPP__
#define __SIHD_UTIL_TRAITS_HPP__

#include <concepts>
#include <type_traits>

namespace sihd::util::traits
{

/**
 * Meta attributes
 */

template <typename T>
concept TriviallyCopyable = std::is_trivially_copyable_v<T>;

template <typename T>
concept Pointer = std::is_pointer_v<T>;

template <typename T>
concept Arithmetic = std::is_arithmetic_v<T>;

template <typename T>
concept Enum = std::is_enum_v<T>;

template <typename T>
concept Integral = std::is_integral_v<T>;

template <typename T>
concept FloatingPoint = std::is_floating_point_v<T>;

template <typename T>
concept Signed = std::is_signed_v<T>;

template <typename T>
concept Unsigned = std::is_unsigned_v<T>;

template <typename T>
concept DefaultConstructible = std::is_default_constructible_v<T>;

template <typename T>
concept CopyConstructible = std::is_copy_constructible_v<T>;

template <typename T>
concept MoveConstructible = std::is_move_constructible_v<T>;

template <typename T>
concept Destructible = std::is_destructible_v<T>;

template <typename T>
concept CopyAssignable = std::is_copy_assignable_v<T>;

template <typename T>
concept MoveAssignable = std::is_move_assignable_v<T>;

template <typename T>
concept Comparable = requires(T a, T b)
{
    {
        a == b
    } -> std::convertible_to<bool>;
    {
        a != b
    } -> std::convertible_to<bool>;
    {
        a < b
    } -> std::convertible_to<bool>;
    {
        a <= b
    } -> std::convertible_to<bool>;
    {
        a > b
    } -> std::convertible_to<bool>;
    {
        a >= b
    } -> std::convertible_to<bool>;
};

template <typename T>
concept EqualityComparable = requires(T a, T b)
{
    {
        a == b
    } -> std::convertible_to<bool>;
    {
        a != b
    } -> std::convertible_to<bool>;
};

template <typename T>
concept LessThanComparable = requires(T a, T b)
{
    {
        a < b
    } -> std::convertible_to<bool>;
};

template <typename T>
concept GreaterThanComparable = requires(T a, T b)
{
    {
        a > b
    } -> std::convertible_to<bool>;
};

template <typename T>
concept LessEqualComparable = requires(T a, T b)
{
    {
        a <= b
    } -> std::convertible_to<bool>;
};

template <typename T>
concept GreaterEqualComparable = requires(T a, T b)
{
    {
        a >= b
    } -> std::convertible_to<bool>;
};

/**
 * Uses in std methods
 */

// template <typename T>
// concept ToStringable = requires(T t)
// {
//     {
//         std::to_string(t)
//     } -> std::convertible_to<std::string>;
// };

template <typename T>
concept Swappable = requires(T a, T b)
{
    std::swap(a, b);
};

template <typename T>
concept Hashable = requires(T a)
{
    {
        std::hash<T> {}(a)
    } -> std::convertible_to<std::size_t>;
};

// template <typename T>
// concept Streamable = requires(T a, std::ostream & os)
// {
//     {
//         os << a
//     } -> std::convertible_to<std::ostream &>;
// };

template <typename T>
concept Callable = requires(T f)
{
    {f()};
};

template <typename F, typename... Args>
concept CallableWith = requires(F f, Args... args)
{
    {f(args...)};
};

/**
 * Containers and methods checking
 */

template <typename T>
concept Container = requires(T t)
{
    typename T::value_type;
    typename T::size_type;
    typename T::iterator;
    typename T::const_iterator;
    t.begin();
    t.end();
    {
        t.size()
    } -> std::convertible_to<std::size_t>;
};

template <typename T>
concept Resizable = requires(T t, typename T::size_type n)
{
    t.resize(n);
};

template <typename T>
concept Reservable = requires(T t, typename T::size_type n)
{
    t.reserve(n);
};

template <typename T>
concept Insertable = requires(T t, typename T::value_type v)
{
    t.insert(v);
};

template <typename T>
concept Erasable = requires(T t, typename T::iterator it)
{
    t.erase(it);
};

template <typename T>
concept BackEmplacable = requires(T t, typename T::value_type v)
{
    t.emplace_back(v);
};

template <typename T>
concept FrontEmplacable = requires(T t, typename T::value_type v)
{
    t.emplace_front(v);
};

template <typename T>
concept Iterable = requires(T t)
{
    std::begin(t);
    std::end(t);
};

template <typename T>
concept HasDataSize = requires(T t)
{
    {
        t.data()
    } -> std::convertible_to<typename std::add_pointer<typename T::value_type>::type>;
    {
        t.size()
    } -> std::convertible_to<std::size_t>;
};

template <typename T>
concept HasConstDataSize = requires(T t)
{
    {
        t.data()
    } -> std::convertible_to<typename std::add_pointer_t<const typename T::value_type>>;
    {
        t.size()
    } -> std::convertible_to<std::size_t>;
};

template <typename T>
concept Map = Iterable<T> && requires(T t)
{
    typename T::key_type;
    typename T::mapped_type;
};

template <typename T>
concept Duration = requires(T t)
{
    typename T::period;
};

template <typename T>
concept StdFunction = requires(T t)
{
    typename T::result_type;
};

// template <typename T>
// concept HasRange = requires(T t)
// {
//     std::ranges::begin(t);
//     std::ranges::end(t);
// };

template <typename T>
concept IsSpan = HasDataSize<T> && requires(T t, size_t offset)
{
    {
        t.subspan(offset)
    } -> std::convertible_to<T>;
};

/**
 * Old SNIFAE
 */

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
# include <type_traits>
# include <algorithm>

namespace sihd::util::traits
{

template <typename, typename = void>
struct IsIterable: public std::false_type {};

template <typename T>
struct IsIterable<T, std::void_t<decltype(std::begin(std::declval<T>())),
                                decltype(std::end(std::declval<T>()))>>: public std::true_type {};

template <typename T>
using enable_if_iterable = typename std::enable_if_t<IsIterable<T>::value, bool>;

template <typename T>
using disable_if_iterable = typename std::enable_if_t<!IsIterable<T>::value, bool>;

template <typename, typename = void>
struct IsMap: public std::false_type {};

template <typename T>
struct IsMap<T, std::void_t<typename T::mapped_type>>: public std::true_type {};

template <typename T>
using enable_if_map = typename std::enable_if_t<IsMap<T>::value, bool>;

template <typename T>
using disable_if_map = typename std::enable_if_t<!IsMap<T>::value, bool>;

}
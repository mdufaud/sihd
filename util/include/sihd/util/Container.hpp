#ifndef __SIHD_UTIL_CONTAINER_HPP__
# define __SIHD_UTIL_CONTAINER_HPP__

# include <type_traits>

namespace sihd::util
{

class Container
{
    public:
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
        struct IsMapContainer: public std::false_type {};

        template <typename T>
        struct IsMapContainer<T, std::void_t<typename T::mapped_type>>: public std::true_type {};

        template <typename T>
        using enable_if_map = typename std::enable_if_t<IsMapContainer<T>::value, bool>;

        template <typename T>
        using disable_if_map = typename std::enable_if_t<!IsMapContainer<T>::value, bool>;

    private:
        Container() {};
};

}

#endif
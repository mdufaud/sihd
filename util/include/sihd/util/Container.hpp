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

        template <typename T, typename V>
        static bool contains(const T & container, V && value)
        {
            return std::find(container.cbegin(), container.cend(), value) != container.cend();
        }

        template <typename T, typename V>
        static bool erase(std::vector<T> & container, V && value)
        {
            auto it = std::remove(container.begin(), container.end(), value);
            bool ret = it != container.end();
            container.erase(it, container.end());
            return ret;
        }

        template <typename T, typename V>
        static bool emplace_unique(std::vector<T> & vec, V && value)
        {
            const auto it = std::find(vec.begin(), vec.end(), value);
            const bool ret = it == vec.end();
            if (ret)
                vec.emplace_back(value);
            return ret;
        }

    private:
        Container() {};
};

}

#endif
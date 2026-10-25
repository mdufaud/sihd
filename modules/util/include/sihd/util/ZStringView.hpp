#ifndef __SIHD_UTIL_ZSTRINGVIEW_HPP__
#define __SIHD_UTIL_ZSTRINGVIEW_HPP__

#include <concepts>
#include <cstddef>
#include <string>
#include <string_view>

namespace sihd::util
{

// a string_view that is always NUL-terminated: never built from a std::string_view
class ZStringView
{
    public:
        constexpr ZStringView(const char *str): _view(str) {}
        constexpr ZStringView(const std::string & str): _view(str) {}
        ZStringView(std::string_view) = delete;
        ZStringView(std::nullptr_t) = delete;

        [[nodiscard]] constexpr const char *c_str() const { return _view.data(); }
        [[nodiscard]] constexpr std::string_view view() const { return _view; }
        [[nodiscard]] constexpr size_t size() const { return _view.size(); }
        [[nodiscard]] constexpr bool empty() const { return _view.empty(); }

        constexpr operator std::string_view() const { return _view; }

        template <typename T>
            requires std::convertible_to<const T &, std::string_view>
        constexpr bool operator==(const T & other) const
        {
            return _view == std::string_view(other);
        }

    private:
        std::string_view _view;
};

constexpr std::string_view format_as(ZStringView str)
{
    return str.view();
}

} // namespace sihd::util

#endif

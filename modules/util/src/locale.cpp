#include <algorithm>

#include <sihd/util/build.hpp>
#include <sihd/util/locale.hpp>

namespace sihd::util::locale
{

std::string normalize_locale_name(std::string_view name)
{
    if (name.empty() || name == "C" || name == "POSIX")
        return std::string(name);

    if constexpr (build::is_windows)
    {
        std::string result(name);

        size_t dot = result.find('.');
        if (dot != std::string::npos)
            result = result.substr(0, dot);

        std::replace(result.begin(), result.end(), '_', '-');

        return result;
    }
    else
        return std::string(name);
}

std::locale create_locale(std::string_view name)
{
    if (name.empty() || name == "C" || name == "POSIX")
        return std::locale::classic();

    // Throws std::runtime_error if locale not available
    // On musl/emscripten: all locales except C will behave like C (no throw but no localization)
    return std::locale(normalize_locale_name(name));
}

} // namespace sihd::util::locale

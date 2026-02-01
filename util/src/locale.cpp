#include <sihd/util/locale.hpp>
#include <sihd/util/platform.hpp>

namespace sihd::util::locale
{

std::string normalize_locale_name(std::string_view name)
{
    if (name.empty() || name == "C" || name == "POSIX")
        return std::string(name);

#if defined(__SIHD_WINDOWS__)
    // Windows: remove codeset (.UTF-8) and replace _ with -
    std::string result(name);

    // Remove codeset (everything after and including the dot)
    size_t dot = result.find('.');
    if (dot != std::string::npos)
        result = result.substr(0, dot);

    // Replace underscore with hyphen (en_US -> en-US)
    std::replace(result.begin(), result.end(), '_', '-');

    return result;
#else
    // POSIX: keep as-is
    return std::string(name);
#endif
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

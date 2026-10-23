#include "internal/desktop_notifier.hpp"

namespace sihd::sys::internal
{

using enum sihd::util::ErrorCode;
using namespace sihd::util;

std::string escape_markup(std::string_view text)
{
    std::string escaped;
    escaped.reserve(text.size());
    for (const char character : text)
    {
        switch (character)
        {
            case '&':
                escaped += "&amp;";
                break;
            case '<':
                escaped += "&lt;";
                break;
            case '>':
                escaped += "&gt;";
                break;
            case '"':
                escaped += "&quot;";
                break;
            case '\'':
                escaped += "&apos;";
                break;
            default:
                escaped += character;
        }
    }
    return escaped;
}

Result<void> check_timeout(const std::optional<Duration> & timeout)
{
    if (timeout.has_value() && *timeout < Duration(0))
        return std::unexpected(Error(invalid_argument, "a desktop notification timeout cannot be negative"));
    return {};
}

} // namespace sihd::sys::internal

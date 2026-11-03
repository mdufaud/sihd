#include "internal/desktop.hpp"

#include <sihd/sys/desktop.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/Url.hpp>
#include <sihd/util/str.hpp>

namespace sihd::sys::desktop
{

using enum sihd::util::ErrorCode;
using namespace sihd::util;

namespace
{

Result<void> check_file_exists(ZStringView path)
{
    if (!fs::exists(path))
        return std::unexpected(Error(not_found, "no such file: '{}'", path));
    return {};
}

} // namespace

Result<void> open_uri(ZStringView uri)
{
    const std::string scheme = Url::decode(uri).scheme;
    // one letter is a windows drive, not a scheme
    if (scheme.size() < 2)
        return std::unexpected(Error(invalid_argument, "not a uri: '{}'", uri));
    if (str::iequals(scheme, "file"))
        return std::unexpected(Error(invalid_argument, "a file:// uri goes through open_file: '{}'", uri));
    return internal::desktop_open_uri(uri);
}

Result<void> open_file(ZStringView path)
{
    auto exists = check_file_exists(path);
    SIHD_UNEXPECTED_RETURN(exists);
    return internal::desktop_open_file(path);
}

Result<void> reveal(ZStringView path)
{
    auto exists = check_file_exists(path);
    SIHD_UNEXPECTED_RETURN(exists);
    return internal::desktop_reveal(path);
}

} // namespace sihd::sys::desktop

#include "sysfs.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>

#include <sihd/sys/fs.hpp>
#include <sihd/util/str.hpp>

using enum sihd::util::ErrorCode;
using namespace sihd::util;

namespace sihd::sys::internal
{

Result<std::string> sysfs_read(std::string_view path)
{
    auto content = fs::read_all(std::string(path));
    SIHD_UNEXPECTED_RETURN(content);
    return std::string(str::trim(*content));
}

Result<void> sysfs_write(std::string_view path, std::string_view value)
{
    const std::string file_path(path);
    const int fd = ::open(file_path.c_str(), O_WRONLY | O_TRUNC | O_CLOEXEC);
    if (fd == -1)
        return std::unexpected(Error::from_errno("could not open '{}'", path));

    ssize_t written;
    while ((written = ::write(fd, value.data(), value.size())) == -1)
    {
        if (errno != EINTR)
        {
            auto error = Error::from_errno("could not write '{}'", path);
            ::close(fd);
            return std::unexpected(std::move(error));
        }
    }
    ::close(fd);
    if (static_cast<size_t>(written) != value.size())
        return std::unexpected(Error(io_error, "short write to '{}'", path));
    return {};
}

} // namespace sihd::sys::internal

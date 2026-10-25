#include <fcntl.h>
#include <unistd.h>

#include <cerrno>

#include <sihd/sys/fs.hpp>
#include <sihd/sys/linux/sysfs.hpp>
#include <sihd/util/str.hpp>

using enum sihd::util::ErrorCode;
using namespace sihd::util;

namespace sihd::sys::linux_::sysfs
{

Result<std::string> read(ZStringView path)
{
    auto content = fs::read_all(path);
    SIHD_UNEXPECTED_RETURN(content);
    return std::string(str::trim(*content));
}

Result<uint64_t> read_uint(ZStringView path)
{
    auto content = read(path);
    SIHD_UNEXPECTED_RETURN(content);
    auto parsed = str::convert_from_string<uint64_t>(*content);
    SIHD_UNEXPECTED_RETURN_CTX(parsed, "reading '{}'", path);
    return parsed;
}

Result<void> write(ZStringView path, std::string_view value)
{
    const int fd = ::open(path.c_str(), O_WRONLY | O_TRUNC | O_CLOEXEC);
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

} // namespace sihd::sys::linux_::sysfs

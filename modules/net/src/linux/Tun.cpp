#include <sihd/util/build.hpp>

#if defined(__SIHD_LINUX__)

# include <fcntl.h>
# include <sys/ioctl.h>
# include <unistd.h>

# include <utility>

# include <sihd/net/Tun.hpp>
# include <sihd/sys/Poll.hpp>
# include <sihd/sys/os.hpp>
# include <sihd/util/Error.hpp>

# include <linux/if_tun.h>
# include <net/if.h>

using enum sihd::util::ErrorCode;
using namespace sihd::util;

namespace sihd::net
{

static_assert(Tun::max_name_size == IFNAMSIZ - 1);

struct Tun::Impl
{
        int fd = -1;
};

Tun::Tun(): _impl(std::make_unique<Impl>()) {}

Tun::Tun(Tun && other): _impl(std::make_unique<Impl>())
{
    std::swap(_impl, other._impl);
}

Tun & Tun::operator=(Tun && other)
{
    if (this != &other)
    {
        this->close();
        std::swap(_impl, other._impl);
    }
    return *this;
}

Tun::~Tun()
{
    this->close();
}

Result<Tun> Tun::adopt(int fd)
{
    if (fd < 0)
        return std::unexpected(Error(invalid_argument, "not a descriptor: {}", fd));
    Tun tun;
    tun._impl->fd = fd;
    return tun;
}

Result<void> Tun::open(const std::string & name)
{
    if (name.empty() || name.size() > max_name_size)
        return std::unexpected(Error(invalid_argument, "tun name must hold 1 to {} characters", max_name_size));

    this->close();
    const int fd = ::open("/dev/net/tun", O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (fd == -1)
        return std::unexpected(Error::from_errno("could not open /dev/net/tun"));

    ifreq request {};
    name.copy(request.ifr_name, max_name_size);
    request.ifr_flags = IFF_TUN | IFF_NO_PI;
    auto created = sihd::sys::os::ioctl(fd, TUNSETIFF, &request);
    if (!created)
    {
        ::close(fd);
        SIHD_UNEXPECTED_RETURN_CTX(created, "creating tun {}", name);
    }
    _impl->fd = fd;
    return {};
}

void Tun::close()
{
    if (_impl->fd != -1)
    {
        ::close(_impl->fd);
        _impl->fd = -1;
    }
}

Result<size_t> Tun::read(void *data, size_t size)
{
    if (_impl->fd == -1)
        return std::unexpected(Error(not_initialized, "tun is not open"));
    const ssize_t ret = ::read(_impl->fd, data, size);
    if (ret == -1)
        return std::unexpected(Error::from_errno("tun read"));
    return static_cast<size_t>(ret);
}

Result<size_t> Tun::write(ArrUByteView packet)
{
    if (_impl->fd == -1)
        return std::unexpected(Error(not_initialized, "tun is not open"));
    const ssize_t ret = ::write(_impl->fd, packet.data(), packet.size());
    if (ret == -1)
        return std::unexpected(Error::from_errno("tun write"));
    return static_cast<size_t>(ret);
}

Result<bool> Tun::wait_readable(Duration timeout) const
{
    if (_impl->fd == -1)
        return std::unexpected(Error(not_initialized, "tun is not open"));
    sihd::sys::Poll poll;
    poll.set_limit(1);
    poll.set_read_fd(_impl->fd);
    poll.poll(timeout.milliseconds_int());
    if (poll.polling_error())
        return std::unexpected(Error::from_errno("tun wait"));
    return !poll.polling_timeout();
}

bool Tun::is_open() const
{
    return _impl->fd != -1;
}

Result<int> Tun::fd() const
{
    if (_impl->fd == -1)
        return std::unexpected(Error(not_initialized, "tun is not open"));
    return _impl->fd;
}

} // namespace sihd::net

#endif

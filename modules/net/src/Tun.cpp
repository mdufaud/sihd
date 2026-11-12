#include <stdexcept>

#include <sihd/net/Tun.hpp>

namespace sihd::net
{

Tun::Tun(int fd): Tun()
{
    auto adopted = this->adopt(fd);
    if (!adopted)
        throw std::runtime_error(adopted.error().message);
}

Tun::Tun(const std::string & name): Tun()
{
    auto opened = this->open(name);
    if (!opened)
        throw std::runtime_error(opened.error().message);
}

} // namespace sihd::net

#include <stdexcept>

#include <sihd/bt/Socket.hpp>

namespace sihd::bt
{

using namespace sihd::util;
using enum sihd::util::ErrorCode;

Socket::Socket(const std::string & address, const std::string & uuid): Socket()
{
    auto connected = this->connect_rfcomm(address, uuid);
    if (!connected)
        throw std::runtime_error(connected.error().message);
}

Socket::Socket(const std::string & address, uint8_t channel): Socket()
{
    auto connected = this->connect_rfcomm_channel(address, channel);
    if (!connected)
        throw std::runtime_error(connected.error().message);
}

Socket::Socket(const std::string & address, AddressType type, uint16_t psm): Socket()
{
    auto connected = this->connect_l2cap(address, type, psm);
    if (!connected)
        throw std::runtime_error(connected.error().message);
}

Socket::Socket(uint16_t psm): Socket()
{
    auto listened = this->listen_l2cap(psm);
    if (!listened)
        throw std::runtime_error(listened.error().message);
}

const std::string & Socket::address() const
{
    return _address;
}

Result<void> Socket::_check_closed() const
{
    if (this->is_listening())
        return std::unexpected(Error(already_exists, "bluetooth socket already listening"));
    if (this->is_open())
        return std::unexpected(Error(already_exists, "bluetooth socket already connected to {}", _address));
    return {};
}

Result<void> Socket::_check_stream() const
{
    if (!this->is_open())
        return std::unexpected(Error(closed, "bluetooth socket is closed"));
    if (this->is_listening())
        return std::unexpected(Error(invalid_argument, "bluetooth socket is listening: accept a client"));
    return {};
}

Result<void> Socket::_check_listening() const
{
    if (!this->is_open())
        return std::unexpected(Error(closed, "bluetooth socket is closed"));
    if (!this->is_listening())
        return std::unexpected(Error(invalid_argument, "bluetooth socket is not listening"));
    return {};
}

} // namespace sihd::bt

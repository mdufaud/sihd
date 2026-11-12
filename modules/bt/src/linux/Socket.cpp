#include <endian.h>
#include <sys/socket.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <deque>
#include <utility>

#include <sihd/bt/Socket.hpp>
#include <sihd/net/Socket.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Clocks.hpp>
#include <sihd/util/str.hpp>

#include "../address.hpp"
#include "bluez.hpp"

namespace sihd::bt
{

using namespace sihd::util;
using enum sihd::util::ErrorCode;
namespace dbus = sihd::sys::linux_::dbus;

namespace
{

// the kernel ABI of <bluetooth/bluetooth.h>, <bluetooth/rfcomm.h> and <bluetooth/l2cap.h>: no libbluetooth needed
constexpr int btproto_l2cap = 0;
constexpr int btproto_rfcomm = 3;
constexpr uint8_t bdaddr_le_public = 1;
constexpr uint8_t bdaddr_le_random = 2;

// stored least significant byte first: AA:BB:CC:DD:EE:FF is {FF, EE, DD, CC, BB, AA}
struct __attribute__((packed)) BdAddr
{
        std::array<uint8_t, 6> bytes;
};

struct SockaddrRc
{
        sa_family_t rc_family;
        BdAddr rc_bdaddr;
        uint8_t rc_channel;
};

struct SockaddrL2
{
        sa_family_t l2_family;
        unsigned short l2_psm;
        BdAddr l2_bdaddr;
        unsigned short l2_cid;
        uint8_t l2_bdaddr_type;
};

constexpr ZStringView profile_path = "/org/sihd/bt/profile";

Result<BdAddr> bdaddr_of(const std::string & address)
{
    auto parsed = parse_address(address);
    SIHD_UNEXPECTED_RETURN(parsed);
    BdAddr bdaddr {};
    for (size_t i = 0; i < bdaddr.bytes.size(); ++i)
        bdaddr.bytes[i] = static_cast<uint8_t>(*parsed >> (8 * i));
    return bdaddr;
}

std::string address_of(const BdAddr & bdaddr)
{
    uint64_t value = 0;
    for (size_t i = bdaddr.bytes.size(); i > 0; --i)
        value = (value << 8) | bdaddr.bytes[i - 1];
    return format_address(value);
}

std::string device_address(const std::string & device_path)
{
    const size_t dev = device_path.rfind("dev_");
    if (dev == std::string::npos)
        return {};
    std::string address = device_path.substr(dev + 4);
    std::ranges::replace(address, '_', ':');
    return address;
}

template <typename Sockaddr>
Result<net::Socket> connect_socket(int type, int protocol, const Sockaddr & peer, const std::string & address)
{
    net::Socket socket;
    auto opened = socket.open(AF_BLUETOOTH, type, protocol);
    SIHD_UNEXPECTED_RETURN_CTX(opened, "opening a bluetooth socket");
    auto connected = socket.connect(reinterpret_cast<const sockaddr *>(&peer), sizeof(peer));
    SIHD_UNEXPECTED_RETURN_CTX(connected, "connecting to {}", address);
    return socket;
}

// each NewConnection hands one connected socket: kept with the address of its device
using Accepted = std::deque<std::pair<net::Socket, std::string>>;

dbus::ObjectServer::Interface profile_interface(Accepted & accepted)
{
    return dbus::ObjectServer::Interface {
        .name = std::string(bluez::profile_interface),
        .methods =
            {
                dbus::ObjectServer::Method {
                    .name = "NewConnection",
                    .in = {{"device", "o"}, {"fd", "h"}, {"fd_properties", "a{sv}"}},
                    .out = {},
                    .handler = [&accepted](dbus::ObjectServer::Call & call) -> Result<void> {
                        dbus::Iter arguments(call.message());
                        auto device = arguments.get<std::string>();
                        SIHD_UNEXPECTED_RETURN(device);
                        arguments.next();
                        auto fd = arguments.get_unix_fd();
                        SIHD_UNEXPECTED_RETURN(fd);
                        auto owned = sys::os::dup(fd->fd());
                        SIHD_UNEXPECTED_RETURN(owned);
                        accepted.emplace_back(net::Socket(*owned, AF_BLUETOOTH, SOCK_STREAM, btproto_rfcomm),
                                              device_address(*device));
                        return call.reply();
                    },
                },
                dbus::ObjectServer::Method {.name = "RequestDisconnection",
                                            .in = {{"device", "o"}},
                                            .out = {},
                                            .handler = [](dbus::ObjectServer::Call & call) { return call.reply(); }},
                dbus::ObjectServer::Method {.name = "Release",
                                            .in = {},
                                            .out = {},
                                            .handler = [](dbus::ObjectServer::Call & call) { return call.reply(); }},
            },
        .properties = {},
        .signals = {},
    };
}

Result<void> register_profile(dbus::Connection & connection,
                              const std::string & uuid,
                              const std::string & role,
                              const std::string & name)
{
    auto registered = bluez::call(
        connection,
        bluez::manager_path,
        bluez::profile_manager_interface,
        "RegisterProfile",
        [&](dbus::Appender & to) -> Result<void> {
            auto path = to.add_object_path(profile_path);
            SIHD_UNEXPECTED_RETURN(path);
            auto uuid_added = to.add(uuid);
            SIHD_UNEXPECTED_RETURN(uuid_added);

            auto options = to.open_array("{sv}");
            SIHD_UNEXPECTED_RETURN(options);
            auto role_added = options->add_entry("Role", "s", [&role](dbus::Appender & value) {
                return value.add(role);
            });
            SIHD_UNEXPECTED_RETURN(role_added);
            auto auto_connect = options->add_entry("AutoConnect", "b", [](dbus::Appender & value) {
                return value.add(false);
            });
            SIHD_UNEXPECTED_RETURN(auto_connect);

            if (!name.empty())
            {
                auto name_added = options->add_entry("Name", "s", [&name](dbus::Appender & value) {
                    return value.add(name);
                });
                SIHD_UNEXPECTED_RETURN(name_added);
            }
            return to.close(std::move(*options));
        },
        bluez::call_timeout);
    SIHD_UNEXPECTED_RETURN(registered);
    return {};
}

Result<std::string> find_device_path(const bluez::ManagedObjects & objects, const std::string & address)
{
    const std::string wanted = str::to_upper_copy(address);
    for (const auto & [path, device] : objects.devices)
    {
        if (str::to_upper_copy(device.result.address) == wanted)
            return path;
    }
    return std::unexpected(Error(not_found, "bluez does not know {}, a scan finds it", address));
}

} // namespace

struct Socket::Impl
{
        Impl() = default;

        explicit Impl(net::Socket stream): socket(std::move(stream)) {}

        // a stream, or the listening kernel socket of an L2CAP listener
        net::Socket socket;
        uint16_t psm = 0;
        bool listening = false;
        std::atomic<bool> open = true;
        // an RFCOMM listener: bluez listens for the profile and hands each connection over dbus
        std::unique_ptr<dbus::Connection> connection;
        std::unique_ptr<dbus::ObjectServer> server;
        Accepted accepted;
};

Socket::Socket() = default;

Socket::Socket(Socket && other) = default;

Socket & Socket::operator=(Socket && other) = default;

Socket::~Socket() = default;

Result<void> Socket::connect_rfcomm(const std::string & address, const std::string & uuid)
{
    auto closed_socket = this->_check_closed();
    SIHD_UNEXPECTED_RETURN(closed_socket);
    dbus::Connection connection;
    auto opened = connection.open(dbus::Bus::system);
    SIHD_UNEXPECTED_RETURN(opened);
    auto objects = bluez::managed_objects(connection);
    SIHD_UNEXPECTED_RETURN(objects);
    auto device_path = find_device_path(*objects, address);
    SIHD_UNEXPECTED_RETURN(device_path);
    auto request = bluez::method_call(*device_path,
                                      bluez::device_interface,
                                      "ConnectProfile",
                                      [&uuid](dbus::Appender & to) { return to.add(uuid); });
    SIHD_UNEXPECTED_RETURN(request);

    // bluez resolves the uuid through SDP, connects, then hands the socket to the client profile
    Accepted accepted;
    dbus::ObjectServer server(connection);
    auto added = server.add_object(std::string(profile_path), {profile_interface(accepted)});
    SIHD_UNEXPECTED_RETURN(added);
    auto registered = register_profile(connection, uuid, "client", "");
    SIHD_UNEXPECTED_RETURN(registered);

    auto connected = server.call(*request, bluez::connect_timeout);
    // the connection leaving the bus unregisters it as well
    (void)bluez::call(
        connection,
        bluez::manager_path,
        bluez::profile_manager_interface,
        "UnregisterProfile",
        [](dbus::Appender & to) { return to.add_object_path(profile_path); },
        bluez::call_timeout);
    if (!connected)
        return std::unexpected(bluez::error_of(connected.error()));
    if (accepted.empty())
        return std::unexpected(Error(io_error, "bluez connected {} without handing its socket", uuid));
    _impl = std::make_unique<Impl>(std::move(accepted.front().first));
    _address = str::to_upper_copy(address);
    return {};
}

Result<void> Socket::connect_rfcomm_channel(const std::string & address, uint8_t channel)
{
    auto closed_socket = this->_check_closed();
    SIHD_UNEXPECTED_RETURN(closed_socket);
    auto bdaddr = bdaddr_of(address);
    SIHD_UNEXPECTED_RETURN(bdaddr);
    SockaddrRc peer {};
    peer.rc_family = AF_BLUETOOTH;
    peer.rc_bdaddr = *bdaddr;
    peer.rc_channel = channel;

    auto socket = connect_socket(SOCK_STREAM, btproto_rfcomm, peer, address);
    SIHD_UNEXPECTED_RETURN(socket);
    _impl = std::make_unique<Impl>(std::move(*socket));
    _address = str::to_upper_copy(address);
    return {};
}

Result<void> Socket::connect_l2cap(const std::string & address, AddressType type, uint16_t psm)
{
    auto closed_socket = this->_check_closed();
    SIHD_UNEXPECTED_RETURN(closed_socket);
    if (type == AddressType::unspecified)
        return std::unexpected(Error(invalid_argument, "an L2CAP LE channel needs the address type of {}", address));
    auto bdaddr = bdaddr_of(address);
    SIHD_UNEXPECTED_RETURN(bdaddr);
    SockaddrL2 peer {};
    peer.l2_family = AF_BLUETOOTH;
    peer.l2_psm = htole16(psm);
    peer.l2_bdaddr = *bdaddr;
    peer.l2_bdaddr_type = type == AddressType::public_address ? bdaddr_le_public : bdaddr_le_random;

    auto socket = connect_socket(SOCK_SEQPACKET, btproto_l2cap, peer, address);
    SIHD_UNEXPECTED_RETURN(socket);
    _impl = std::make_unique<Impl>(std::move(*socket));
    _impl->psm = psm;
    _address = str::to_upper_copy(address);
    return {};
}

Result<void> Socket::listen_rfcomm(const std::string & uuid, const std::string & name)
{
    auto closed_socket = this->_check_closed();
    SIHD_UNEXPECTED_RETURN(closed_socket);
    auto impl = std::make_unique<Impl>();
    impl->listening = true;
    impl->connection = std::make_unique<dbus::Connection>();
    auto opened = impl->connection->open(dbus::Bus::system);
    SIHD_UNEXPECTED_RETURN(opened);
    impl->server = std::make_unique<dbus::ObjectServer>(*impl->connection);
    auto added = impl->server->add_object(std::string(profile_path), {profile_interface(impl->accepted)});
    SIHD_UNEXPECTED_RETURN(added);

    // bluez picks a free channel and publishes the SDP record until this connection leaves the bus
    auto registered = register_profile(*impl->connection, uuid, "server", name);
    SIHD_UNEXPECTED_RETURN(registered);
    _impl = std::move(impl);
    _address.clear();
    return {};
}

Result<void> Socket::listen_l2cap(uint16_t psm)
{
    auto closed_socket = this->_check_closed();
    SIHD_UNEXPECTED_RETURN(closed_socket);
    auto impl = std::make_unique<Impl>();
    impl->listening = true;
    auto opened = impl->socket.open(AF_BLUETOOTH, SOCK_SEQPACKET, btproto_l2cap);
    SIHD_UNEXPECTED_RETURN_CTX(opened, "opening a bluetooth socket");

    SockaddrL2 local {};
    local.l2_family = AF_BLUETOOTH;
    local.l2_psm = htole16(psm);
    local.l2_bdaddr_type = bdaddr_le_public;
    auto bound_psm = impl->socket.bind(reinterpret_cast<const sockaddr *>(&local), sizeof(local));
    SIHD_UNEXPECTED_RETURN_CTX(bound_psm, "binding the L2CAP psm {}", psm);
    auto listened = impl->socket.listen(SOMAXCONN);
    SIHD_UNEXPECTED_RETURN_CTX(listened, "listening on the L2CAP psm {}", psm);

    SockaddrL2 bound {};
    socklen_t length = sizeof(bound);
    if (::getsockname(impl->socket, reinterpret_cast<sockaddr *>(&bound), &length) < 0)
        return std::unexpected(Error::from_errno("could not read the L2CAP psm"));
    impl->psm = le16toh(bound.l2_psm);
    _impl = std::move(impl);
    _address.clear();
    return {};
}

bool Socket::is_open() const
{
    return _impl != nullptr && _impl->open;
}

bool Socket::is_listening() const
{
    return this->is_open() && _impl->listening;
}

uint16_t Socket::psm() const
{
    return _impl != nullptr ? _impl->psm : 0;
}

Result<size_t> Socket::send(ArrUByteView data)
{
    auto streaming = this->_check_stream();
    SIHD_UNEXPECTED_RETURN(streaming);
    auto sent = _impl->socket.send_all(ArrCharView(reinterpret_cast<const char *>(data.data()), data.size()));
    SIHD_UNEXPECTED_RETURN_CTX(sent, "sending to {}", _address);
    return data.size();
}

Result<size_t> Socket::receive(IArray & array)
{
    auto streaming = this->_check_stream();
    SIHD_UNEXPECTED_RETURN(streaming);
    auto received = _impl->socket.receive(array);
    SIHD_UNEXPECTED_RETURN_CTX(received, "receiving from {}", _address);
    return received;
}

Result<Socket> Socket::accept(Duration timeout)
{
    auto listening = this->_check_listening();
    SIHD_UNEXPECTED_RETURN(listening);

    Socket client;
    if (_impl->socket.is_open())
    {
        SockaddrL2 peer {};
        socklen_t length = sizeof(peer);
        auto fd = _impl->socket.accept(reinterpret_cast<sockaddr *>(&peer), &length, timeout.milliseconds_int());
        if (!fd)
        {
            auto still_listening = this->_check_listening();
            SIHD_UNEXPECTED_RETURN(still_listening);
            if (fd.error().code == ErrorCode::timeout)
                return std::unexpected(Error(ErrorCode::timeout, "no L2CAP connection on psm {}", _impl->psm));
            SIHD_UNEXPECTED_RETURN_CTX(fd, "accepting on the L2CAP psm {}", _impl->psm);
        }
        client._impl = std::make_unique<Impl>(net::Socket(*fd, AF_BLUETOOTH, SOCK_SEQPACKET, btproto_l2cap));
        client._impl->psm = _impl->psm;
        client._address = address_of(peer.l2_bdaddr);
        return client;
    }

    SteadyClock clock;
    const Timestamp deadline = clock.now() + timeout;
    dbus::Message message;

    while (_impl->accepted.empty())
    {
        const Timestamp now = clock.now();
        if (now >= deadline)
            return std::unexpected(Error(ErrorCode::timeout, "no RFCOMM connection"));
        auto received = message.receive(*_impl->connection, Duration(deadline - now));
        SIHD_UNEXPECTED_RETURN(received);
        auto still_listening = this->_check_listening();
        SIHD_UNEXPECTED_RETURN(still_listening);
        if (!*received)
            continue;
        auto handled = _impl->server->handle(std::move(message));
        SIHD_UNEXPECTED_RETURN(handled);
    }

    auto [stream, address] = std::move(_impl->accepted.front());
    _impl->accepted.pop_front();
    client._impl = std::make_unique<Impl>(std::move(stream));
    client._address = str::to_upper_copy(address);
    return client;
}

void Socket::close()
{
    if (_impl == nullptr || !_impl->open.exchange(false))
        return;
    if (_impl->socket.is_open())
        (void)_impl->socket.shutdown();
    else
        (void)_impl->connection->interrupt();
}

} // namespace sihd::bt

#include <winsock2.h>

#include <ws2bth.h>

#include <atomic>

#include <sihd/bt/Socket.hpp>
#include <sihd/net/Socket.hpp>
#include <sihd/sys/EventPoll.hpp>
#include <sihd/util/str.hpp>

#include "../address.hpp"
#include "winrt.hpp"

namespace sihd::bt
{

using namespace sihd::util;
using enum sihd::util::ErrorCode;

namespace
{

// the listening backlog of an RFCOMM server
constexpr int backlog = 4;

Result<SOCKADDR_BTH> peer_of(const std::string & address)
{
    auto bth = parse_address(address);
    SIHD_UNEXPECTED_RETURN(bth);
    SOCKADDR_BTH peer {};
    peer.addressFamily = AF_BTH;
    peer.btAddr = *bth;
    return peer;
}

Result<net::Socket> connect_to(const SOCKADDR_BTH & peer, const std::string & address)
{
    net::Socket socket;
    auto opened = socket.open(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
    SIHD_UNEXPECTED_RETURN_CTX(opened, "opening a bluetooth socket");
    auto connected = socket.connect(reinterpret_cast<const sockaddr *>(&peer), sizeof(peer));
    SIHD_UNEXPECTED_RETURN_CTX(connected, "connecting to {}", address);
    return socket;
}

} // namespace

struct Socket::Impl
{
        Impl() = default;

        explicit Impl(net::Socket stream): socket(std::move(stream)) {}

        ~Impl()
        {
            if (registered)
                WSASetServiceW(&record, RNRSERVICE_DELETE, 0);
        }

        Impl(const Impl &) = delete;
        Impl & operator=(const Impl &) = delete;

        net::Socket socket;
        bool listening = false;
        std::atomic<bool> open = true;

        // a listener: close() ends its wait
        std::unique_ptr<sys::EventPoll> poll;
        // the SDP record of a listener, its pointers on the fields below
        WSAQUERYSETW record {};
        CSADDR_INFO address {};
        SOCKADDR_BTH local {};
        GUID uuid {};
        std::wstring name;
        bool registered = false;
};

Socket::Socket() = default;

Socket::Socket(Socket && other) = default;

Socket & Socket::operator=(Socket && other) = default;

Socket::~Socket()
{
    this->close();
}

Result<void> Socket::connect_rfcomm(const std::string & address, const std::string & uuid)
{
    auto closed_socket = this->_check_closed();
    SIHD_UNEXPECTED_RETURN(closed_socket);
    auto peer = peer_of(address);
    SIHD_UNEXPECTED_RETURN(peer);
    auto service = winrt::guid_of(uuid);
    SIHD_UNEXPECTED_RETURN(service);
    // windows looks the channel up in the SDP records of the device
    peer->serviceClassId = *service;

    auto socket = connect_to(*peer, address);
    SIHD_UNEXPECTED_RETURN(socket);
    _impl = std::make_unique<Impl>(std::move(*socket));
    _address = str::to_upper_copy(address);
    return {};
}

Result<void> Socket::connect_rfcomm_channel(const std::string & address, uint8_t channel)
{
    auto closed_socket = this->_check_closed();
    SIHD_UNEXPECTED_RETURN(closed_socket);
    auto peer = peer_of(address);
    SIHD_UNEXPECTED_RETURN(peer);
    peer->port = channel;
    auto socket = connect_to(*peer, address);
    SIHD_UNEXPECTED_RETURN(socket);
    _impl = std::make_unique<Impl>(std::move(*socket));
    _address = str::to_upper_copy(address);
    return {};
}

Result<void> Socket::connect_l2cap(const std::string &, AddressType, uint16_t)
{
    return std::unexpected(Error(not_supported, "windows has no user mode L2CAP channel"));
}

Result<void> Socket::listen_rfcomm(const std::string & uuid, const std::string & name)
{
    auto closed_socket = this->_check_closed();
    SIHD_UNEXPECTED_RETURN(closed_socket);
    auto service = winrt::guid_of(uuid);
    SIHD_UNEXPECTED_RETURN(service);
    auto impl = std::make_unique<Impl>();
    impl->listening = true;
    impl->uuid = *service;
    impl->name = str::to_wstr(name);
    impl->local.addressFamily = AF_BTH;
    impl->local.port = BT_PORT_ANY;

    auto opened = impl->socket.open(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
    SIHD_UNEXPECTED_RETURN_CTX(opened, "RFCOMM listener");
    auto bound = impl->socket.bind(reinterpret_cast<const sockaddr *>(&impl->local), sizeof(impl->local));
    SIHD_UNEXPECTED_RETURN_CTX(bound, "RFCOMM listener");
    auto listened = impl->socket.listen(backlog);
    SIHD_UNEXPECTED_RETURN_CTX(listened, "RFCOMM listener");
    int length = sizeof(impl->local);
    if (::getsockname(static_cast<int>(impl->socket), reinterpret_cast<sockaddr *>(&impl->local), &length) != 0)
        return std::unexpected(net::Socket::make_error("RFCOMM listener"));

    impl->address.LocalAddr.lpSockaddr = reinterpret_cast<sockaddr *>(&impl->local);
    impl->address.LocalAddr.iSockaddrLength = sizeof(impl->local);
    impl->address.iSocketType = SOCK_STREAM;
    impl->address.iProtocol = BTHPROTO_RFCOMM;
    impl->record.dwSize = sizeof(impl->record);
    impl->record.lpszServiceInstanceName = impl->name.data();
    impl->record.lpServiceClassId = &impl->uuid;
    impl->record.dwNameSpace = NS_BTH;
    impl->record.dwNumberOfCsAddrs = 1;
    impl->record.lpcsaBuffer = &impl->address;

    impl->registered = WSASetServiceW(&impl->record, RNRSERVICE_REGISTER, 0) == 0;
    if (!impl->registered)
        return std::unexpected(net::Socket::make_error("RFCOMM service record"));
    impl->poll = std::make_unique<sys::EventPoll>();
    if (!impl->poll->set_read_fd(impl->socket))
        return std::unexpected(Error(io_error, "RFCOMM listener: the socket cannot be polled"));
    _impl = std::move(impl);
    _address.clear();
    return {};
}

Result<void> Socket::listen_l2cap(uint16_t)
{
    return std::unexpected(Error(not_supported, "windows has no user mode L2CAP channel"));
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
    return 0;
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
    if (!received)
    {
        auto still_streaming = this->_check_stream();
        SIHD_UNEXPECTED_RETURN(still_streaming);
        SIHD_UNEXPECTED_RETURN_CTX(received, "receiving from {}", _address);
    }
    return received;
}

Result<Socket> Socket::accept(Duration timeout)
{
    auto listening = this->_check_listening();
    SIHD_UNEXPECTED_RETURN(listening);
    const int ready = _impl->poll->poll(timeout.milliseconds_int());
    auto still_listening = this->_check_listening();
    SIHD_UNEXPECTED_RETURN(still_listening);
    if (ready < 0)
        return std::unexpected(Error(io_error, "could not poll the RFCOMM listener"));
    if (ready == 0)
        return std::unexpected(Error(ErrorCode::timeout, "no RFCOMM connection"));

    SOCKADDR_BTH peer {};
    socklen_t length = sizeof(peer);
    auto accepted = _impl->socket.accept(reinterpret_cast<sockaddr *>(&peer), &length);
    SIHD_UNEXPECTED_RETURN_CTX(accepted, "accepting an RFCOMM connection");
    Socket client;
    client._impl = std::make_unique<Impl>(net::Socket(*accepted, AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM));
    client._address = format_address(peer.btAddr);
    return client;
}

void Socket::close()
{
    if (_impl == nullptr || !_impl->open.exchange(false))
        return;
    if (_impl->listening)
    {
        _impl->poll->request_stop();
        return;
    }
    (void)_impl->socket.shutdown();
    // a blocking receive of another thread is an io of the socket handle
    CancelIoEx(reinterpret_cast<HANDLE>(static_cast<uintptr_t>(static_cast<int>(_impl->socket))), nullptr);
}

} // namespace sihd::bt

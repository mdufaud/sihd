#ifndef __SIHD_BT_SOCKET_HPP__
#define __SIHD_BT_SOCKET_HPP__

#include <cstdint>
#include <memory>
#include <string>

#include <sihd/bt/adapter.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/IArray.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::bt
{

// an RFCOMM stream or L2CAP LE credit based channel, connected or listening for remote devices
class Socket
{
    public:
        Socket();
        Socket(const std::string & address, const std::string & uuid);
        Socket(const std::string & address, uint8_t channel);
        Socket(const std::string & address, AddressType type, uint16_t psm);
        // listens on that L2CAP psm
        explicit Socket(uint16_t psm);
        Socket(Socket && other);
        Socket & operator=(Socket && other);
        ~Socket();

        Socket(const Socket &) = delete;
        Socket & operator=(const Socket &) = delete;

        // the uuid of an SDP record the device publishes
        sihd::util::Result<void> connect_rfcomm(const std::string & address, const std::string & uuid);
        sihd::util::Result<void> connect_rfcomm_channel(const std::string & address, uint8_t channel);
        sihd::util::Result<void> connect_l2cap(const std::string & address, AddressType type, uint16_t psm);
        // publishes an SDP record of that uuid and name
        sihd::util::Result<void> listen_rfcomm(const std::string & uuid, const std::string & name);
        // psm 0 lets the system pick one
        sihd::util::Result<void> listen_l2cap(uint16_t psm);

        const std::string & address() const;
        bool is_open() const;
        bool is_listening() const;
        // the L2CAP psm connected or listened on, 0 otherwise
        uint16_t psm() const;

        // sends every byte
        sihd::util::Result<size_t> send(sihd::util::ArrUByteView data);
        // up to the array capacity, 0 once the peer closed
        sihd::util::Result<size_t> receive(sihd::util::IArray & array);
        // the stream of the next device, a timeout error once it elapsed, closed once close() ran
        sihd::util::Result<Socket> accept(sihd::util::Duration timeout);
        // wakes a receive or an accept blocked in another thread
        void close();

    private:
        struct Impl;

        sihd::util::Result<void> _check_closed() const;
        sihd::util::Result<void> _check_stream() const;
        sihd::util::Result<void> _check_listening() const;

        std::unique_ptr<Impl> _impl;
        std::string _address;
};

} // namespace sihd::bt

#endif

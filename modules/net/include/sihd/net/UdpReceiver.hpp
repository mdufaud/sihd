#ifndef __SIHD_NET_UDPRECEIVER_HPP__
#define __SIHD_NET_UDPRECEIVER_HPP__

#include <expected>

#include <sihd/net/INetReceiver.hpp>
#include <sihd/net/Socket.hpp>
#include <sihd/sys/Poll.hpp>
#include <sihd/util/ABlockingService.hpp>
#include <sihd/util/Configurable.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Named.hpp>

namespace sihd::net
{

class UdpReceiver: public INetReceiver,
                   public sihd::util::Named,
                   public sihd::util::Configurable,
                   public sihd::util::ABlockingService,
                   public sihd::util::Observable<INetReceiver>,
                   public sihd::util::IHandler<sihd::sys::Poll *>
{
    public:
        UdpReceiver(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~UdpReceiver();

        std::expected<void, sihd::util::Error> open_socket(bool ipv6 = false);
        std::expected<void, sihd::util::Error> open_socket_unix();

        bool socket_opened() { return _socket.is_open(); }

        std::expected<void, sihd::util::Error> bind(const IpAddr & addr);
        std::expected<void, sihd::util::Error> bind_unix(std::string_view path);

        std::expected<void, sihd::util::Error> open_and_bind(const IpAddr & ip);
        std::expected<void, sihd::util::Error> open_and_bind(std::string_view ip, int port);
        std::expected<void, sihd::util::Error> open_unix_and_bind(std::string_view path);

        std::expected<size_t, sihd::util::Error> receive(void *buf, size_t len);
        std::expected<size_t, sihd::util::Error> receive(IpAddr & addr, void *buf, size_t len);

        // INetReceiver
        std::expected<void, sihd::util::Error> close() override;
        std::expected<size_t, sihd::util::Error> receive(sihd::util::IArray & arr) override;
        std::expected<size_t, sihd::util::Error> receive(IpAddr & addr, sihd::util::IArray & arr) override;

        bool set_poll_timeout(int milliseconds);
        // poll for x milliseconds - returns true if socket is read
        bool poll(int milliseconds);
        // poll once with configured timeout
        bool poll();

        // to set blocking/broadcast
        const Socket & socket() const { return _socket; }

    protected:
        void handle(sihd::sys::Poll *poll) override;
        bool on_start() override;
        bool on_stop() override;

    private:
        void _setup_poll();

        Socket _socket;
        std::mutex _poll_mutex;
        sihd::sys::Poll _poll;
};

} // namespace sihd::net

#endif
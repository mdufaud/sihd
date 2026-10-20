#ifndef __SIHD_NET_UDPRECEIVER_HPP__
#define __SIHD_NET_UDPRECEIVER_HPP__

#include <sihd/net/INetReceiver.hpp>
#include <sihd/net/Socket.hpp>
#include <sihd/sys/Poll.hpp>
#include <sihd/util/ABlockingService.hpp>
#include <sihd/util/Configurable.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Named.hpp>
#include <sihd/util/Result.hpp>

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

        sihd::util::Result<void> open_socket(bool ipv6 = false);
        sihd::util::Result<void> open_socket_unix();

        bool socket_opened() { return _socket.is_open(); }

        bool set_reuseaddr(bool active);

        sihd::util::Result<void> bind(const IpAddr & addr);
        sihd::util::Result<void> bind_unix(std::string_view path);

        sihd::util::Result<void> open_and_bind(const IpAddr & ip);
        sihd::util::Result<void> open_and_bind(std::string_view ip, int port);
        sihd::util::Result<void> open_unix_and_bind(std::string_view path);

        sihd::util::Result<size_t> receive(void *buf, size_t len);
        sihd::util::Result<size_t> receive(IpAddr & addr, void *buf, size_t len);

        // INetReceiver
        sihd::util::Result<void> close() override;
        sihd::util::Result<size_t> receive(sihd::util::IArray & arr) override;
        sihd::util::Result<size_t> receive(IpAddr & addr, sihd::util::IArray & arr) override;

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
        bool _reuseaddr;
        std::mutex _poll_mutex;
        sihd::sys::Poll _poll;
};

} // namespace sihd::net

#endif
#ifndef __SIHD_NET_TCPCLIENT_HPP__
#define __SIHD_NET_TCPCLIENT_HPP__

#include <sihd/net/INetReceiver.hpp>
#include <sihd/net/INetSender.hpp>
#include <sihd/net/Socket.hpp>

#include <sihd/util/ABlockingService.hpp>
#include <sihd/util/Configurable.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/IRunnable.hpp>
#include <sihd/util/Named.hpp>
#include <sihd/util/Observable.hpp>
#include <sihd/util/Poll.hpp>
#include <sihd/util/Waitable.hpp>

namespace sihd::net
{

class TcpClient: public INetReceiver,
                 public INetSender,
                 public sihd::util::Named,
                 public sihd::util::Configurable,
                 public sihd::util::ABlockingService,
                 public sihd::util::Observable<INetReceiver>,
                 public sihd::util::IHandler<sihd::util::Poll *>
{
    public:
        TcpClient(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~TcpClient();

        bool open_socket(bool ipv6 = false);
        bool open_socket_unix();
        bool socket_opened() { return _socket.is_open(); }

        bool connect(const IpAddr & addr);
        bool connect(std::string_view path);

        bool open_and_connect(const IpAddr & ip);
        bool open_unix_and_connect(std::string_view path);

        bool close();

        bool set_poll_timeout(int milliseconds);
        // poll for x milliseconds - returns true if socket is read
        bool poll(int milliseconds);
        // poll once with configured timeout
        bool poll();

        ssize_t receive(IpAddr & addr, sihd::util::IArray & arr);
        ssize_t receive(sihd::util::IArray & arr);

        ssize_t receive(void *buf, size_t len);

        ssize_t send(sihd::util::ArrCharView view);
        bool send_all(sihd::util::ArrCharView view);

        // to set blocking/broadcast
        const Socket & socket() const { return _socket; }
        bool connected() const { return _connected; }
        const IpAddr & client_addr() const { return _client_addr; }

    protected:
        void handle(sihd::util::Poll *poll) override;
        bool on_start() override;
        bool on_stop() override;

    private:
        void _setup_poll();

        Socket _socket;
        IpAddr _client_addr;
        bool _connected;
        std::mutex _poll_mutex;
        sihd::util::Poll _poll;
};

} // namespace sihd::net

#endif
#ifndef __SIHD_NET_DEVTCPSERVER_HPP__
#define __SIHD_NET_DEVTCPSERVER_HPP__

#include <sihd/core/Device.hpp>
#include <sihd/net/BasicServerHandler.hpp>
#include <sihd/net/TcpServer.hpp>
#include <sihd/util/IRunnable.hpp>
#include <sihd/util/Worker.hpp>

namespace sihd::net
{

// rx and tx carry raw payloads from and to every client, rx_from and tx_to carry encoded ClientFrame
class DevTcpServer: public sihd::core::Device,
                    public sihd::util::IHandler<BasicServerHandler *>,
                    protected sihd::util::IRunnable
{
    public:
        DevTcpServer(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~DevTcpServer();

        bool is_running() const override;

        bool set_host(std::string_view host);
        bool set_port(int port);
        bool set_unix_path(std::string_view path);
        bool set_buffer_capacity(size_t capacity);
        bool set_poll_timeout(int milliseconds);
        bool set_poll_limit(int limit);
        bool set_queue_size(size_t size);
        bool set_max_clients(size_t max);

    protected:
        using sihd::core::Device::handle;

        void handle(sihd::core::Channel *c) override;
        void handle(BasicServerHandler *handler) override;

        bool on_init() override;
        bool on_start() override;
        bool on_stop() override;
        bool on_reset() override;

        bool run() override;

    private:
        void _send_to_one(const sihd::core::Channel *c);

        TcpServer _tcp_server;
        BasicServerHandler _server_handler;
        sihd::util::Worker _worker;

        std::string _host;
        int _port;
        std::string _unix_path;
        size_t _buffer_capacity;

        sihd::core::Channel *_channel_rx;
        sihd::core::Channel *_channel_tx;
        sihd::core::Channel *_channel_client_count;
        sihd::core::Channel *_channel_rx_from;
        sihd::core::Channel *_channel_tx_to;
        sihd::core::Channel *_channel_connected;
        sihd::core::Channel *_channel_disconnected;
};

} // namespace sihd::net

#endif

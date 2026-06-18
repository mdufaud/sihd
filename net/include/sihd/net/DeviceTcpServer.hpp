#ifndef __SIHD_NET_DEVICETCPSERVER_HPP__
#define __SIHD_NET_DEVICETCPSERVER_HPP__

#include <sihd/core/Device.hpp>
#include <sihd/net/BasicServerHandler.hpp>
#include <sihd/net/TcpServer.hpp>
#include <sihd/util/IRunnable.hpp>
#include <sihd/util/Worker.hpp>

namespace sihd::net
{

class DeviceTcpServer: public sihd::core::Device,
                       public sihd::util::IHandler<BasicServerHandler *>,
                       protected sihd::util::IRunnable
{
    public:
        DeviceTcpServer(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~DeviceTcpServer();

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
};

} // namespace sihd::net

#endif

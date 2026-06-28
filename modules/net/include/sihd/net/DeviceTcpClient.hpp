#ifndef __SIHD_NET_DEVICETCPCLIENT_HPP__
#define __SIHD_NET_DEVICETCPCLIENT_HPP__

#include <sihd/core/Device.hpp>
#include <sihd/net/INetReceiver.hpp>
#include <sihd/net/TcpClient.hpp>
#include <sihd/util/IRunnable.hpp>
#include <sihd/util/Synchronizer.hpp>
#include <sihd/util/Worker.hpp>

namespace sihd::net
{

class DeviceTcpClient: public sihd::core::Device,
                       public sihd::util::IHandler<INetReceiver *>,
                       protected sihd::util::IRunnable
{
    public:
        DeviceTcpClient(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~DeviceTcpClient();

        bool is_running() const override;

        bool set_host(std::string_view host);
        bool set_port(int port);
        bool set_unix_path(std::string_view path);
        bool set_buffer_capacity(size_t capacity);
        bool set_poll_timeout(int milliseconds);
        bool set_connect_timeout(int milliseconds);
        bool set_reconnect_interval(int milliseconds);

    protected:
        using sihd::core::Device::handle;

        void handle(sihd::core::Channel *c) override;
        void handle(INetReceiver *receiver) override;

        bool on_init() override;
        bool on_start() override;
        bool on_stop() override;
        bool on_reset() override;

        bool run() override;

    private:
        bool _connect();

        TcpClient _tcp_client;
        sihd::util::Worker _worker;
        sihd::util::Synchronizer _start_sync;
        std::atomic<bool> _stop_requested;
        bool _start_ok;

        std::string _host;
        int _port;
        std::string _unix_path;
        size_t _buffer_capacity;
        int _connect_timeout;
        int _reconnect_interval;

        sihd::core::Channel *_channel_rx;
        sihd::core::Channel *_channel_tx;
        sihd::core::Channel *_channel_connected;
};

} // namespace sihd::net

#endif

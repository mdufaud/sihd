#ifndef __SIHD_NET_DEVICEUDPRECEIVER_HPP__
#define __SIHD_NET_DEVICEUDPRECEIVER_HPP__

#include <barrier>

#include <sihd/core/Device.hpp>
#include <sihd/net/INetReceiver.hpp>
#include <sihd/net/UdpReceiver.hpp>
#include <sihd/util/IRunnable.hpp>
#include <sihd/util/Worker.hpp>

namespace sihd::net
{

class DeviceUdpReceiver: public sihd::core::Device,
                         public sihd::util::IHandler<INetReceiver *>,
                         protected sihd::util::IRunnable
{
    public:
        DeviceUdpReceiver(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~DeviceUdpReceiver();

        bool is_running() const override;

        bool set_host(std::string_view host);
        bool set_port(int port);
        bool set_buffer_capacity(size_t capacity);
        bool set_poll_timeout(int milliseconds);

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
        UdpReceiver _udp_receiver;
        sihd::util::Worker _worker;
        std::barrier<> _start_barrier;
        std::atomic<bool> _stop_requested;
        bool _start_ok;

        std::string _host;
        int _port;
        size_t _buffer_capacity;

        sihd::core::Channel *_channel_rx;
};

} // namespace sihd::net

#endif

#ifndef __SIHD_NET_DEVICEUDPSENDER_HPP__
#define __SIHD_NET_DEVICEUDPSENDER_HPP__

#include <sihd/core/Device.hpp>
#include <sihd/net/UdpSender.hpp>

namespace sihd::net
{

class DeviceUdpSender: public sihd::core::Device
{
    public:
        DeviceUdpSender(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~DeviceUdpSender();

        bool is_running() const override;

        bool set_host(std::string_view host);
        bool set_port(int port);
        bool set_unix_path(std::string_view path);

    protected:
        using sihd::core::Device::handle;

        void handle(sihd::core::Channel *c) override;

        bool on_init() override;
        bool on_start() override;
        bool on_stop() override;
        bool on_reset() override;

    private:
        UdpSender _udp_sender;

        std::string _host;
        int _port;
        std::string _unix_path;

        sihd::core::Channel *_channel_tx;
};

} // namespace sihd::net

#endif

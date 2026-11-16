#ifndef __SIHD_NET_LOGGERREADERUDP_HPP__
#define __SIHD_NET_LOGGERREADERUDP_HPP__

#include <string>
#include <string_view>

#include <sihd/net/INetReceiver.hpp>
#include <sihd/net/LoggerEndpoint.hpp>
#include <sihd/net/UdpReceiver.hpp>
#include <sihd/util/ABlockingService.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/Configurable.hpp>
#include <sihd/util/IHandler.hpp>
#include <sihd/util/Named.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::net
{

class LoggerReaderUdp: public sihd::util::Named,
                       public sihd::util::Configurable,
                       public sihd::util::ABlockingService,
                       public sihd::util::IHandler<INetReceiver *>
{
    public:
        static constexpr size_t max_record_size = 64 * 1024;

        LoggerReaderUdp(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~LoggerReaderUdp();

        bool set_host(std::string_view host);
        bool set_port(int port);
        bool set_unix_path(std::string_view path);

        sihd::util::Result<int> local_port() const;

        bool is_ready() const override;
        bool wait_ready(sihd::util::Duration timeout) const override;

    protected:
        void handle(INetReceiver *receiver) override;
        bool on_start() override;
        bool on_stop() override;

    private:
        UdpReceiver _receiver;
        sihd::util::ArrByte _buffer;
        LoggerEndpoint _endpoint;
};

} // namespace sihd::net

#endif

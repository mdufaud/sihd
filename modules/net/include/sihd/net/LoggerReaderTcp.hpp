#ifndef __SIHD_NET_LOGGERREADERTCP_HPP__
#define __SIHD_NET_LOGGERREADERTCP_HPP__

#include <cstdint>
#include <map>
#include <string>
#include <string_view>

#include <sihd/net/BasicServerHandler.hpp>
#include <sihd/net/LoggerEndpoint.hpp>
#include <sihd/net/TcpServer.hpp>
#include <sihd/util/ABlockingService.hpp>
#include <sihd/util/Configurable.hpp>
#include <sihd/util/IHandler.hpp>
#include <sihd/util/Named.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::net
{

class LoggerReaderTcp: public sihd::util::Named,
                       public sihd::util::Configurable,
                       public sihd::util::ABlockingService,
                       public sihd::util::IHandler<BasicServerHandler *>
{
    public:
        static constexpr size_t max_record_size = 16 * 1024 * 1024;

        LoggerReaderTcp(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~LoggerReaderTcp();

        bool set_host(std::string_view host);
        bool set_port(int port);
        bool set_unix_path(std::string_view path);

        sihd::util::Result<int> local_port() const;

        bool is_ready() const override;
        bool wait_ready(sihd::util::Duration timeout) const override;

    protected:
        void handle(BasicServerHandler *handler) override;
        bool on_start() override;
        bool on_stop() override;

    private:
        bool _read_records(const BasicServerHandler::ClientPtr & client);
        void _close_clients();

        TcpServer _server;
        BasicServerHandler _handler;
        std::map<uint64_t, std::string> _pending;
        LoggerEndpoint _endpoint;
};

} // namespace sihd::net

#endif

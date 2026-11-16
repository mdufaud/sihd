#ifndef __SIHD_NET_ALOGGERWRITER_HPP__
#define __SIHD_NET_ALOGGERWRITER_HPP__

#include <atomic>
#include <optional>
#include <string>
#include <string_view>

#include <sihd/net/LoggerEndpoint.hpp>
#include <sihd/net/LoggerRecord.hpp>
#include <sihd/net/Socket.hpp>
#include <sihd/util/ALogger.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/Clocks.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/Timestamp.hpp>

namespace sihd::net
{

// log() runs under the LoggerManager lock: it never blocks past the timeouts and never logs
class ALoggerWriter: public sihd::util::ALogger
{
    public:
        virtual ~ALoggerWriter();

        void log(const sihd::util::LogInfo & info, std::string_view msg) override;

        bool set_host(std::string_view host);
        bool set_port(int port);
        bool set_unix_path(std::string_view path);
        bool set_reconnect_interval(int milliseconds);
        bool set_connect_timeout(int milliseconds);
        bool set_send_timeout(int milliseconds);

        bool is_connected() const;

    protected:
        ALoggerWriter(const std::string & name, sihd::util::Node *parent, int socket_type);

        virtual sihd::util::ArrByte encode_frame(const LoggerRecord & record) const = 0;

    private:
        bool _endpoint_changed(const sihd::util::Result<void> & changed);
        bool _is_configured() const;
        void _establish_at_conf();
        bool _establish();
        sihd::util::Result<void> _connect();
        sihd::util::Result<void> _open_socket();
        void _disconnect();
        void _warn_outage(std::string_view reason);

        const int _socket_type;
        LoggerEndpoint _endpoint;
        int _reconnect_interval;
        int _connect_timeout;
        int _send_timeout;
        Socket _socket;
        sihd::util::SteadyClock _clock;
        std::optional<sihd::util::Timestamp> _failed_at;
        std::atomic<bool> _connected;
        bool _warned_unconfigured;
        bool _warned_outage;
};

} // namespace sihd::net

#endif

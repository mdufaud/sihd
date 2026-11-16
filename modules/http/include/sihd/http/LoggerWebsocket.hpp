#ifndef __SIHD_HTTP_LOGGERWEBSOCKET_HPP__
#define __SIHD_HTTP_LOGGERWEBSOCKET_HPP__

#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/http/AWebsocketHandler.hpp>
#include <sihd/http/HttpServer.hpp>
#include <sihd/util/ALogger.hpp>
#include <sihd/util/LogFormatter.hpp>

namespace sihd::http
{

class LoggerWebsocket: public sihd::util::ALogger
{
    public:
        LoggerWebsocket(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~LoggerWebsocket();

        void log(const sihd::util::LogInfo & info, std::string_view msg) override;

        bool set_pattern(const std::string & pattern);

        // the logger must outlive the server
        bool serve(HttpServer & server, const char *protocol);

        size_t connection_count() const;

    private:
        mutable std::mutex _mutex;
        std::vector<AWebsocketHandler *> _connections;
        sihd::util::LogFormatter _formatter;
};

} // namespace sihd::http

#endif

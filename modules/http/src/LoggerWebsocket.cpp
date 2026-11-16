#include <fmt/format.h>

#include <sihd/http/LoggerWebsocket.hpp>
#include <sihd/util/LoggerManager.hpp>

namespace sihd::http
{

using namespace sihd::util;

namespace
{

class LoggerWebsocketClient: public AWebsocketHandler
{
    public:
        LoggerWebsocketClient(IWebsocketConnection & connection,
                              std::mutex & mutex,
                              std::vector<AWebsocketHandler *> & connections):
            AWebsocketHandler(connection),
            _mutex(mutex),
            _connections(connections)
        {
            std::lock_guard lock(_mutex);
            _connections.push_back(this);
        }

        ~LoggerWebsocketClient()
        {
            std::lock_guard lock(_mutex);
            std::erase(_connections, this);
        }

        void on_text(std::string_view) override {}

    private:
        std::mutex & _mutex;
        std::vector<AWebsocketHandler *> & _connections;
};

} // namespace

LoggerWebsocket::LoggerWebsocket(const std::string & name, Node *parent): ALogger(name, parent)
{
    this->add_conf("pattern", &LoggerWebsocket::set_pattern);
}

LoggerWebsocket::~LoggerWebsocket() = default;

bool LoggerWebsocket::set_pattern(const std::string & pattern)
{
    if (pattern.empty())
        return true;
    auto patterned = _formatter.set_pattern(pattern);
    return patterned.has_value();
}

bool LoggerWebsocket::serve(HttpServer & server, const char *protocol)
{
    return server.add_websocket<LoggerWebsocketClient>(protocol, _mutex, _connections);
}

size_t LoggerWebsocket::connection_count() const
{
    std::lock_guard lock(_mutex);
    return _connections.size();
}

void LoggerWebsocket::log(const LogInfo & info, std::string_view msg)
{
    std::lock_guard lock(_mutex);
    if (_connections.empty())
        return;
    const std::string line = _formatter.format(info, msg);
    for (AWebsocketHandler *connection : _connections)
    {
        if (!connection->send(line) && connection->dropped() == 1)
            LoggerManager::sink_warning(
                this->name(),
                fmt::format("a '{}' connection queue is full, dropping messages", connection->protocol_name()));
    }
}

} // namespace sihd::http

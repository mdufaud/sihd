#ifndef __SIHD_HTTP_WEBSOCKETCONNECTION_HPP__
#define __SIHD_HTTP_WEBSOCKETCONNECTION_HPP__

#include <atomic>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include <sihd/http/HttpRequest.hpp>
#include <sihd/http/IWebsocketConnection.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/SafeQueue.hpp>

#include "../WriteProtocol.hpp"
#include "../lws.hpp"

namespace sihd::http
{

class WebsocketConnection: public IWebsocketConnection
{
    public:
        struct Message
        {
                std::string data;
                WriteProtocol protocol = WriteProtocol::Text;
        };

        WebsocketConnection(struct lws_context *context,
                            const std::string & protocol_name,
                            size_t max_pending,
                            HttpRequest && request);
        ~WebsocketConnection();

        WebsocketConnection(const WebsocketConnection &) = delete;
        WebsocketConnection & operator=(const WebsocketConnection &) = delete;

        sihd::util::Result<void> send(std::string_view text) override;
        sihd::util::Result<void> send_binary(sihd::util::ArrByteView data) override;
        void close() override;
        bool is_connected() const override;
        size_t dropped() const override;
        std::string_view protocol_name() const override;
        const HttpRequest & request() const override;
        HttpRequest & request();

        std::optional<Message> pop();
        bool has_pending() const;
        bool close_requested() const;
        void disconnect();

    private:
        sihd::util::Result<void> _push(Message && message);

        struct lws_context *_context;
        const std::string _protocol_name;
        const size_t _max_pending;
        HttpRequest _request;
        sihd::util::SafeQueue<Message> _pending;
        std::atomic<bool> _connected {true};
        std::atomic<bool> _close_requested {false};
        std::atomic<bool> _wake_requested {false};
        std::atomic<size_t> _dropped {0};
};

} // namespace sihd::http

#endif

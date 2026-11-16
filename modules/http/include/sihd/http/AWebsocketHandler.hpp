#ifndef __SIHD_HTTP_AWEBSOCKETHANDLER_HPP__
#define __SIHD_HTTP_AWEBSOCKETHANDLER_HPP__

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <sihd/http/HttpRequest.hpp>
#include <sihd/http/IWebsocketConnection.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::http
{

class AWebsocketHandler
{
    public:
        AWebsocketHandler(IWebsocketConnection & connection);
        virtual ~AWebsocketHandler();

        AWebsocketHandler(const AWebsocketHandler &) = delete;
        AWebsocketHandler & operator=(const AWebsocketHandler &) = delete;

        sihd::util::Result<void> send(std::string_view text);
        sihd::util::Result<void> send_binary(sihd::util::ArrByteView data);
        void close();
        bool is_connected() const;
        size_t dropped() const;
        std::string_view protocol_name() const;
        const HttpRequest & request() const;

        // false refuses the upgrade: the client never gets the websocket
        virtual bool on_auth(HttpRequest & request);
        virtual void on_open();
        virtual void on_text(std::string_view text) = 0;
        virtual void on_binary(sihd::util::ArrByteView data);
        virtual void on_peer_close(uint16_t code, std::string_view reason);
        virtual void on_close();

    private:
        IWebsocketConnection & _connection;
};

} // namespace sihd::http

#endif

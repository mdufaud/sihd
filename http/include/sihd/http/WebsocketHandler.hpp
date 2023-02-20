#ifndef __SIHD_HTTP_WEBSOCKETHANDLER_HPP__
#define __SIHD_HTTP_WEBSOCKETHANDLER_HPP__

#include <functional>

#include <sihd/http/IWebsocketHandler.hpp>

namespace sihd::http
{

class WebsocketHandler: public sihd::http::IWebsocketHandler
{
    public:
        WebsocketHandler();
        virtual ~WebsocketHandler();

        void on_open(std::string_view protocol_number);
        bool on_read(const sihd::util::ArrChar & array);
        bool on_write(sihd::util::ArrChar & array, LwsWriteProtocol & protocol);
        void on_close();

        std::function<void(std::string_view)> callback_open;
        std::function<bool(const sihd::util::ArrChar &)> callback_read;
        std::function<bool(sihd::util::ArrChar &, LwsWriteProtocol &)> callback_write;
        std::function<void()> callback_close;

    protected:

    private:
};

} // namespace sihd::http

#endif
#ifndef __SIHD_HTTP_WEBSOCKETHANDLER_HPP__
# define __SIHD_HTTP_WEBSOCKETHANDLER_HPP__

# include <sihd/http/IWebsocketHandler.hpp>
# include <functional>

namespace sihd::http
{

class WebsocketHandler: public sihd::http::IWebsocketHandler
{
    public:
        WebsocketHandler();
        virtual ~WebsocketHandler();

		void on_open(const char *protocol_number);
		bool on_read(const sihd::util::ArrChar & array);
		bool on_write(sihd::util::ArrChar & array, LwsWriteProtocol *protocol);
		void on_close();

        std::function<void(const char *)> callback_open;
        std::function<bool(const sihd::util::ArrChar &)> callback_read;
        std::function<bool(sihd::util::ArrChar &, LwsWriteProtocol *)> callback_write;
        std::function<void()> callback_close;

    protected:

    private:
};

}

#endif
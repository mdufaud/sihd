#ifndef __SIHD_HTTP_HTTPSERVER_HPP__
#define __SIHD_HTTP_HTTPSERVER_HPP__

#include <functional>
#include <memory>
#include <set>

#include <sihd/http/AWebsocketHandler.hpp>
#include <sihd/http/HttpRequest.hpp>
#include <sihd/http/HttpResponse.hpp>
#include <sihd/http/IHttpAuthenticator.hpp>
#include <sihd/http/IHttpFilter.hpp>
#include <sihd/http/IWebsocketConnection.hpp>
#include <sihd/http/MimeTypes.hpp>
#include <sihd/http/WebService.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/ABlockingService.hpp>
#include <sihd/util/Configurable.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::http
{

class HttpServer: public sihd::util::Node,
                  public sihd::util::Configurable,
                  public sihd::util::ABlockingService
{
    public:
        HttpServer(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~HttpServer();

        bool set_encoding(std::string_view encoding);
        bool set_port(int port);
        bool set_root_dir(const std::string & root_dir);
        bool set_ssl_cert_path(const std::string & path);
        bool set_ssl_cert_key(const std::string & path);
        bool set_404_path(std::string_view path);
        bool set_server_name(const std::string & name);
        bool set_cors_origin(std::string_view origin);
        // loopback by default, empty means every interface
        bool set_interface(std::string_view iface);
        // 0 means no limit
        bool set_max_request_size(size_t size);
        // a websocket message past it closes its connection, 0 means no limit
        bool set_max_message_size(size_t size);
        // messages a websocket connection queues before refusing writes, 0 means no limit
        bool set_websocket_max_pending(size_t count);

        void set_http_filter(IHttpFilter *filter);
        void set_authenticator(IHttpAuthenticator *authenticator);
        bool set_service_thread_count(size_t count);

        void request_stop();
        // the port the server listens on once ready, the one picked by the system when set to 0
        int get_port() const;

        bool add_resource_path(const std::string & path);
        bool remove_resource_path(const std::string & path);

        virtual sihd::util::Result<std::string> get_resource_path(std::string_view path);

        // each connection gets its own T(IWebsocketConnection &, args...): args must outlive the server
        template <typename T, typename... Args>
        bool add_websocket(const char *name, Args &...args)
        {
            return this->_add_websocket(name, [&args...](IWebsocketConnection & connection) {
                return std::make_unique<T>(connection, args...);
            });
        }
        // "*" allows any; unlisted origins pass only as the literal host the server was reached at
        bool add_websocket_origin(std::string_view origin);

    protected:
        bool on_start() override;
        bool on_stop() override;

    private:
        using WebsocketFactory = std::function<std::unique_ptr<AWebsocketHandler>(IWebsocketConnection &)>;

        bool _add_websocket(const char *name, const WebsocketFactory & factory);

        struct Impl;
        std::unique_ptr<Impl> _impl;
};

} // namespace sihd::http

#endif

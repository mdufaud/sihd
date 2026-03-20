#ifndef __SIHD_HTTP_HTTPSERVER_HPP__
#define __SIHD_HTTP_HTTPSERVER_HPP__

#include <memory>
#include <set>

#include <sihd/util/ABlockingService.hpp>
#include <sihd/util/Configurable.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/platform.hpp>

#include <sihd/http/HttpRequest.hpp>
#include <sihd/http/HttpResponse.hpp>
#include <sihd/http/IHttpAuthenticator.hpp>
#include <sihd/http/IHttpFilter.hpp>
#include <sihd/http/IWebsocketHandler.hpp>
#include <sihd/http/Mime.hpp>
#include <sihd/http/WebService.hpp>

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
        bool set_root_dir(std::string_view root_dir);
        bool set_poll_frequency(double freq);
        bool set_ssl_cert_path(std::string_view path);
        bool set_ssl_cert_key(std::string_view path);
        bool set_404_path(std::string_view path);
        bool set_server_name(std::string_view path);
        bool set_cors_origin(std::string_view origin);

        void set_http_filter(IHttpFilter *filter);
        void set_authenticator(IHttpAuthenticator *authenticator);
        bool set_service_thread_count(size_t count);

        void request_stop();

        bool add_resource_path(const std::string & path);
        bool remove_resource_path(const std::string & path);

        virtual bool get_resource_path(std::string_view path, std::string & res);

        bool add_websocket(const char *name, IWebsocketHandler *handler, size_t tx_packet_size = 0);

    protected:
        bool on_start() override;
        bool on_stop() override;

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
};

} // namespace sihd::http

#endif

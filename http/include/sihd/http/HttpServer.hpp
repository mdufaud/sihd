#ifndef __SIHD_HTTP_HTTPSERVER_HPP__
# define __SIHD_HTTP_HTTPSERVER_HPP__

# include <sihd/util/Node.hpp>
# include <sihd/util/IStoppableRunnable.hpp>
# include <sihd/util/Configurable.hpp>
# include <sihd/util/StepWorker.hpp>
# include <sihd/util/Waitable.hpp>
# include <sihd/util/Array.hpp>

# include <sihd/http/Mime.hpp>
# include <sihd/http/IWebsocketHandler.hpp>
# include <sihd/http/WebService.hpp>
# include <sihd/http/HttpRequest.hpp>
# include <sihd/http/HttpResponse.hpp>
# include <sihd/http/HttpHeader.hpp>

# include <mutex>
# include <set>
# include <libwebsockets.h>

namespace sihd::http
{

LOGGER;

class HttpServer:   public sihd::util::Node,
                    public sihd::util::Configurable,
                    public sihd::util::IRunnable
{
    public:
        HttpServer(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~HttpServer();

        bool set_encoding(const std::string & encoding);
        bool set_port(int port);
        bool set_root_dir(const std::string & root_dir);
        bool set_poll_frequency(double freq);
        bool set_ssl_cert_path(const std::string & path);
        bool set_ssl_cert_key(const std::string & path);
        bool add_resource_path(const std::string & path);
        bool remove_resource_path(const std::string & path);

        virtual bool run();
        virtual bool stop();

        virtual bool get_resource_path(const std::string & path, std::string & res);

        const struct lws *current_lws() const { return _lws_current_wsi_ptr; }

    protected:
        struct HttpSession
        {
            void clear_request()
            {
                if (request != nullptr)
                {
                    delete request;
                    request = nullptr;
                }
                if (content != nullptr)
                {
                    delete content;
                    content = nullptr;
                }
                content_size = 0;
            }

            void new_request()
            {
                request = nullptr;
                content = nullptr;
                content_size = 0;
                should_complete_transaction = true;
            }

            // forward lws callback args
            struct lws *wsi;
            void *in;
            size_t len;
            int rc;
            // http request
            HttpRequest::RequestType request_type;
            size_t content_size;
            sihd::util::ArrUByte *content;
            HttpRequest *request;
            bool should_complete_transaction;
        };

        struct WebsocketSession
        {
            const lws_protocols *protocol;
            struct lws *wsi;
            void *in;
            size_t len;
        };

        // http protocol callbacks
        static int _global_http_lws_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);
        int _lws_http_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

        virtual int _on_http_request(HttpSession *session, const std::string & path);
        virtual bool _check_webservices(HttpSession *session, const std::string & path);
        virtual bool _serve_webservice(HttpSession *session, WebService *webservice, const HttpRequest & request);

        HttpRequest::RequestType _get_request_type(struct lws *wsi);

        // websocket protocol callbacks
        static int _global_websocket_lws_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);
        int _lws_websocket_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

        virtual int _on_websocket_open(IWebsocketHandler *handler, WebsocketSession *session);
        virtual int _on_websocket_read(IWebsocketHandler *handler, WebsocketSession *session);
        virtual int _on_websocket_write(IWebsocketHandler *handler, WebsocketSession *session);
        virtual int _on_websocket_close(IWebsocketHandler *handler, WebsocketSession *session);

        // add protocols
        virtual bool _add_protocol(const char *name, lws_callback_function *callback, size_t struct_size, size_t tx_packet_size = 0);
        virtual bool _add_websocket(const char *name, IWebsocketHandler *handler, size_t tx_packet_size = 0);

        // polling protocols call
        virtual bool _check_all_protocols();

        virtual bool _send_http_headers(struct lws *wsi, HttpHeader & header);
        virtual bool _send_404(struct lws *wsi);

        virtual const char *_get_client_ip(struct lws *wsi);
        std::vector<std::string> _get_uri_args(struct lws *wsi);

    private:
        WebService *_get_webservice_from_path(const std::string & path, std::string *webservice_name = nullptr);

        struct LwsPollingScheduler: public sihd::util::IRunnable
        {
            LwsPollingScheduler(HttpServer *server);
            ~LwsPollingScheduler();

            bool run();

            HttpServer *server;
        };

        char _ip_buf[INET6_ADDRSTRLEN];
        bool _running;
        int _port;
        std::string _root_dir;
        std::set<std::string> _resources_path;
        std::string _ssl_cert_path;
        std::string _ssl_cert_key;
        std::mutex _mutex;

        lws_http_mount *_lws_mount_ptr;
        lws_context *_lws_context_ptr;
        size_t _protocols_count;
        lws_protocols *_lws_protocols_ptr;
        struct lws *_lws_current_wsi_ptr;

        std::vector<IWebsocketHandler *> _websocket_handler_lst;
        std::string _encoding;
        HttpHeader _http_header;
        std::string _404_content;

        // mimetype database
        Mime _mime;
        // poll handler
        LwsPollingScheduler _polling_scheduler;
        sihd::util::StepWorker _worker;
        sihd::util::Waitable _waitable;
};

}

#endif
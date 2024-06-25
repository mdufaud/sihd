#ifndef __SIHD_HTTP_HTTPSERVER_HPP__
#define __SIHD_HTTP_HTTPSERVER_HPP__

#include <set>

#include <sihd/util/ABlockingService.hpp>
#include <sihd/util/Configurable.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/StepWorker.hpp>
#include <sihd/util/Waitable.hpp>
#include <sihd/util/os.hpp>

#include <sihd/http/HttpHeader.hpp>
#include <sihd/http/HttpRequest.hpp>
#include <sihd/http/HttpResponse.hpp>
#include <sihd/http/IWebsocketHandler.hpp>
#include <sihd/http/Mime.hpp>
#include <sihd/http/WebService.hpp>

// forward declarations of libwebsockets to prevent header leaking
struct lws;
struct lws_protocols;
typedef int lws_callback_function(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);
struct lws_http_mount;
struct lws_context;

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

        bool add_resource_path(const std::string & path);
        bool remove_resource_path(const std::string & path);

        virtual bool run();
        bool is_running() const { return _running; }
        virtual bool stop();
        virtual void wait_stop();

        virtual bool get_resource_path(std::string_view path, std::string & res);

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
                // current http request type
                HttpRequest::RequestType request_type;
                // current http body content
                sihd::util::ArrUByte *content;
                // current http body size
                size_t content_size;
                // current HttpRequest with uri args
                HttpRequest *request;
                // if true lws_http_transaction_completed will be called
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
        static int _global_http_lws_callback(struct lws *wsi,
                                             enum lws_callback_reasons reason,
                                             void *user,
                                             void *in,
                                             size_t len);
        int _lws_http_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

        HttpRequest::RequestType _get_request_type(struct lws *wsi);

        virtual int _on_http_request(HttpSession *session, std::string_view path);
        virtual int _on_http_body(HttpSession *session, const uint8_t *buf, size_t size);
        virtual int _on_http_body_end(HttpSession *session);
        virtual int _on_http_request_end();
        virtual int _on_http_file_completion_end();

        virtual bool _check_webservices(HttpSession *session, std::string_view path);
        virtual bool _serve_webservice(HttpSession *session, WebService *webservice, HttpRequest & request);

        // websocket protocol callbacks
        static int _global_websocket_lws_callback(struct lws *wsi,
                                                  enum lws_callback_reasons reason,
                                                  void *user,
                                                  void *in,
                                                  size_t len);
        int _lws_websocket_callback(struct lws *wsi,
                                    enum lws_callback_reasons reason,
                                    void *user,
                                    void *in,
                                    size_t len);

        virtual int _on_websocket_open(IWebsocketHandler *handler, WebsocketSession *session);
        virtual int _on_websocket_read(IWebsocketHandler *handler, WebsocketSession *session);
        virtual int _on_websocket_write(IWebsocketHandler *handler, WebsocketSession *session);
        virtual int _on_websocket_close(IWebsocketHandler *handler, WebsocketSession *session);

        // protocols
        virtual bool _add_protocol(const char *name,
                                   lws_callback_function *callback,
                                   size_t struct_size,
                                   size_t tx_packet_size = 0);
        virtual bool _add_websocket(const char *name, IWebsocketHandler *handler, size_t tx_packet_size = 0);

        // polling protocols call
        virtual bool _check_all_protocols();

        virtual bool _send_http_headers(struct lws *wsi, HttpResponse & response);
        virtual bool _send_http_no_content(struct lws *wsi, int code);
        /*
            code = HTTP_STATUS_MOVED_PERMANENTLY || HTTP_STATUS_FOUND || HTTP_STATUS_SEE_OTHER ||
           HTTP_STATUS_NOT_MODIFIED default = 301 HTTP_STATUS_MOVED_PERMANENTLY
        */
        virtual bool _send_http_redirect(struct lws *wsi, std::string_view redirect_path, int code = 301);
        virtual bool _send_404(struct lws *wsi, std::string_view html_404);

        std::optional<std::string> _get_header(struct lws *wsi, enum lws_token_indexes idx);
        std::string _get_client_ip(struct lws *wsi);
        std::vector<std::string> _get_uri_args(struct lws *wsi);

    private:
        WebService *_get_webservice_from_path(std::string_view path, std::string *webservice_name = nullptr);

        struct LwsPollingScheduler: public sihd::util::IRunnable
        {
                LwsPollingScheduler(HttpServer *server);
                ~LwsPollingScheduler();

                bool run();

                HttpServer *server;
        };

        std::atomic<bool> _running;
        std::atomic<bool> _stop;
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

        std::vector<IWebsocketHandler *> _websocket_handler_lst;
        std::string _encoding;

        HttpResponse _http_response;
        sihd::util::ArrUByte _http_header_array;

        std::string _404_page_path;

        // mimetype database
        Mime _mime;
        // poll handler
        LwsPollingScheduler _polling_scheduler;
        sihd::util::StepWorker _worker;
        sihd::util::Waitable _waitable;
};

} // namespace sihd::http

#endif

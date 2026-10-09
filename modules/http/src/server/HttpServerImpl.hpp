#ifndef __SIHD_HTTP_SRC_SERVER_HTTPSERVERIMPL_HPP__
#define __SIHD_HTTP_SRC_SERVER_HTTPSERVERIMPL_HPP__

#include <atomic>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <sihd/http/HttpRequest.hpp>
#include <sihd/http/HttpResponse.hpp>
#include <sihd/http/HttpServer.hpp>
#include <sihd/http/HttpStatus.hpp>
#include <sihd/http/IHttpAuthenticator.hpp>
#include <sihd/http/IHttpFilter.hpp>
#include <sihd/http/IWebsocketHandler.hpp>
#include <sihd/http/MimeTypes.hpp>
#include <sihd/http/WebService.hpp>
#include <sihd/http/WriteProtocol.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/StepWorker.hpp>
#include <sihd/util/str.hpp>

#include "../lws.hpp"

#ifndef SIHD_HTTP_URI_BUFSIZE
# define SIHD_HTTP_URI_BUFSIZE 512
#endif

#ifndef SIHD_HTTP_HEADER_VALUE_BUFSIZE
# define SIHD_HTTP_HEADER_VALUE_BUFSIZE 2048
#endif

#ifndef SIHD_HTTP_HEADERS_BUFSIZE
# define SIHD_HTTP_HEADERS_BUFSIZE (LWS_PRE + LWS_RECOMMENDED_MIN_HEADER_SPACE)
#endif

namespace sihd::http
{

int write_protocol_to_lws(WriteProtocol protocol);

struct HttpServer::Impl
{
        struct HttpSession
        {
                void new_request()
                {
                    request.reset();
                    // a keep-alive connection must not pin the largest body it served
                    if (content.capacity() > max_retained_body_capacity)
                        content = sihd::util::ArrChar {};
                    else
                        content.clear();
                    body_stream.reset();
                    stream_provider = nullptr;
                    cached_auth_header.reset();
                    content_length = 0;
                    content_size = 0;
                    request_type = HttpRequest::None;
                    rc = 0;
                    stream_sent = 0;
                    stream_length = 0;
                    waiting_body = false;
                    response_pending = false;
                    reject_body = false;
                    close_after_stream = false;
                }

                struct lws *wsi = nullptr;
                void *in = nullptr;
                size_t len = 0;
                int rc = 0;
                HttpRequest::RequestType request_type = HttpRequest::None;
                sihd::util::ArrChar content;
                size_t content_length = 0;
                size_t content_size = 0;
                std::unique_ptr<IBodyStream> body_stream;
                std::unique_ptr<HttpRequest> request;
                HttpResponse::StreamProvider stream_provider;
                size_t stream_sent = 0;
                size_t stream_length = 0;
                std::optional<std::string> cached_auth_header;
                // lws expects the request body, the transaction cannot end yet
                bool waiting_body = false;
                // the response writes past this callback: a stream or a served file
                bool response_pending = false;
                bool reject_body = false;
                bool close_after_stream = false;
                // one way: a body the server refused leaves the connection unusable, the
                // response is flushed and the connection closed instead of desyncing it
                bool close_connection = false;

                static constexpr size_t max_retained_body_capacity = 64 * 1024;
        };

        struct WebsocketSession
        {
                const lws_protocols *protocol;
                struct lws *wsi;
                void *in;
                size_t len;
        };

        struct LwsPollingScheduler: public sihd::util::IRunnable
        {
                LwsPollingScheduler(HttpServer *srv): server(srv) {}
                ~LwsPollingScheduler() = default;

                bool run()
                {
                    server->_impl->check_all_protocols();
                    return true;
                }

                HttpServer *server;
        };

        Impl(HttpServer *server);
        ~Impl();

        // session management
        void session_init(HttpSession *session);
        void session_cleanup(HttpSession *session);

        // static lws callbacks
        static int _global_http_lws_callback(struct lws *wsi,
                                             enum lws_callback_reasons reason,
                                             void *user,
                                             void *in,
                                             size_t len);
        static int _global_websocket_lws_callback(struct lws *wsi,
                                                  enum lws_callback_reasons reason,
                                                  void *user,
                                                  void *in,
                                                  size_t len);

        // http callbacks & helpers
        int _lws_http_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);
        HttpRequest::RequestType get_request_type(struct lws *wsi);
        bool is_cors_preflight(struct lws *wsi);
        int on_http_request(HttpSession *session, std::string_view path);
        int on_http_body(HttpSession *session, const uint8_t *buf, size_t size);
        int on_http_body_end(HttpSession *session);
        // ends the lws transaction, or drops the connection when the body was refused
        int complete_transaction(HttpSession *session, int rc);
        bool check_stream_length(HttpSession *session);
        WebService *_get_webservice_from_path(std::string_view path, std::string *webservice_name = nullptr);
        bool check_webservices(HttpSession *session, std::string_view path);
        // returns the announced body length, or nothing once the body was refused
        std::optional<size_t>
            resolve_body_length(HttpSession *session, HttpRequest & request, const std::string & webservice_name);
        // installs the body stream, false once the body was refused
        bool prepare_body_stream(HttpSession *session,
                                 WebService *webservice,
                                 HttpRequest & request,
                                 size_t content_length,
                                 const std::string & webservice_name);
        bool serve_bodyless(HttpSession *session, WebService *webservice, HttpRequest & request);
        int serve_body(HttpSession *session);
        // fills the request fields the stream providers and handlers may read while the body streams in
        void populate_request(HttpSession *session, HttpRequest & request);
        // answers a request no route matched, from either the bodyless or the body path
        bool serve_not_found(HttpSession *session, std::string_view path);

        struct AuthResult
        {
                bool authorized = false;
                std::string user;
                std::string token;
        };

        static std::unordered_map<std::string, std::string>
            parse_query_params(const std::vector<std::string> & uri_args);
        bool check_and_apply_auth(HttpSession *session, WebService *webservice, HttpRequest & request);
        bool send_response(HttpSession *session, HttpResponse & response);
        bool serve_webservice(HttpSession *session, WebService *webservice, HttpRequest & request);
        AuthResult parse_authorization(std::string_view auth_header_value, IHttpAuthenticator *authenticator);
        IHttpAuthenticator *get_authenticator_for(WebService *webservice);

        // header/response helpers
        std::optional<std::string> get_header(struct lws *wsi, enum lws_token_indexes idx);
        // name must carry its trailing ':' as lws matches it verbatim
        std::optional<std::string> get_custom_header(struct lws *wsi, std::string_view name);
        void apply_request_headers(struct lws *wsi, HttpRequest & request);
        std::string get_client_ip(struct lws *wsi);
        std::vector<std::string> get_uri_args(struct lws *wsi);
        bool send_404(struct lws *wsi, std::string_view html_404);
        // header-only error response, allow is the method list of a 405
        bool send_http_error(HttpSession *session, int code, std::string_view allow = {});
        // refuses the body: the response carries connection: close and the transaction
        // ends by dropping the connection instead of desyncing it
        bool reject_body(HttpSession *session, int code);
        bool reject_body(HttpSession *session, HttpResponse & response);
        bool send_http_redirect(struct lws *wsi, std::string_view redirect_path, int code = 301);
        bool send_http_headers(struct lws *wsi, HttpResponse & response);

        // websocket callbacks
        int _lws_websocket_callback(struct lws *wsi,
                                    enum lws_callback_reasons reason,
                                    void *user,
                                    void *in,
                                    size_t len);
        int on_websocket_write(IWebsocketHandler *handler, WebsocketSession *session);

        // protocol management
        bool add_protocol(const char *name, lws_callback_function *callback, size_t struct_size, size_t tx_packet_size);
        bool check_all_protocols();

        // members
        HttpServer *server;

        std::atomic<bool> stop;
        int port;
        std::string root_dir;
        std::set<std::string> resources_path;
        std::string ssl_cert_path;
        std::string ssl_cert_key;
        std::mutex mutex;

        lws_http_mount *lws_mount_ptr;
        lws_context *lws_context_ptr;
        size_t protocols_count;
        lws_protocols *lws_protocols_ptr;

        std::vector<IWebsocketHandler *> websocket_handler_lst;
        IHttpFilter *http_filter;
        IHttpAuthenticator *http_authenticator;
        std::string encoding;
        std::string default_server_name;
        std::string default_cors_origin;

        std::string page_404_path;

        size_t max_request_size = 0;
        // a buffered body is read fully in memory: streaming is the path for bigger payloads
        size_t max_buffered_request_size = 16 * 1024 * 1024;

        MimeTypes mime;
        LwsPollingScheduler polling_scheduler;
        sihd::util::StepWorker stepworker;

        size_t service_thread_count = 1;
        std::vector<std::thread> service_threads;

        std::mutex sessions_mutex;
        std::unordered_set<HttpSession *> active_sessions;
};

} // namespace sihd::http

#endif

#include <libwebsockets.h>

#include <thread>

#include <sihd/http/HttpServer.hpp>
#include <sihd/http/HttpStatus.hpp>
#include <sihd/http/WriteProtocol.hpp>
#include <sihd/sys/NamedFactory.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/StepWorker.hpp>

#ifndef SIHD_HTTP_URI_BUFSIZE
# define SIHD_HTTP_URI_BUFSIZE 512
#endif

#ifndef SIHD_HTTP_HEADER_VALUE_BUFSIZE
# define SIHD_HTTP_HEADER_VALUE_BUFSIZE 512
#endif

#ifndef SIHD_HTTP_HEADERS_BUFSIZE
# define SIHD_HTTP_HEADERS_BUFSIZE (LWS_PRE + LWS_RECOMMENDED_MIN_HEADER_SPACE)
#endif

namespace sihd::http
{

SIHD_REGISTER_FACTORY(HttpServer)

SIHD_NEW_LOGGER("sihd::http");

using namespace sihd::util;
using namespace sihd::sys;

// defined in WriteProtocol.cpp
int write_protocol_to_lws(WriteProtocol protocol);

struct HttpServer::Impl
{
        struct HttpSession
        {
                void clear_request()
                {
                    request.reset();
                    content.reset();
                    content_size = 0;
                }

                void new_request()
                {
                    request.reset();
                    content.reset();
                    content_size = 0;
                    should_complete_transaction = true;
                    stream_provider = nullptr;
                    if (cached_auth_header)
                        cached_auth_header->clear();
                }

                // forward lws callback args
                struct lws *wsi;
                void *in;
                size_t len;
                int rc;
                // current http request type
                HttpRequest::RequestType request_type;
                // current http body content
                std::unique_ptr<ArrUByte> content;
                // current http body size
                size_t content_size;
                // current HttpRequest with uri args
                std::unique_ptr<HttpRequest> request;
                // if true lws_http_transaction_completed will be called
                bool should_complete_transaction;
                // streaming state
                HttpResponse::StreamProvider stream_provider;
                // cached auth header for deferred POST/PUT (heap-allocated for lws zero-init safety)
                std::unique_ptr<std::string> cached_auth_header;
        };

        struct WebsocketSession
        {
                const lws_protocols *protocol;
                struct lws *wsi;
                void *in;
                size_t len;
        };

        struct LwsPollingScheduler: public IRunnable
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

        Impl(HttpServer *server):
            server(server),
            stop(false),
            port(0),
            lws_mount_ptr(nullptr),
            lws_context_ptr(nullptr),
            protocols_count(0),
            lws_protocols_ptr(nullptr),
            http_filter(nullptr),
            http_authenticator(nullptr),
            polling_scheduler(server)
        {
            stepworker.set_runnable(&polling_scheduler);
            stepworker.set_frequency(10);

            default_server_name = server->name();
            default_cors_origin = "*";
            encoding = "utf-8";

            this->add_protocol("http-server", Impl::_global_http_lws_callback, sizeof(HttpSession), 0);
        }

        ~Impl()
        {
            std::lock_guard l(mutex);
            if (lws_protocols_ptr != nullptr)
            {
                free(lws_protocols_ptr);
                lws_protocols_ptr = nullptr;
            }
            if (lws_context_ptr != nullptr)
            {
                lws_context_destroy(lws_context_ptr);
                lws_context_ptr = nullptr;
            }
        }

        // static lws callbacks
        static int _global_http_lws_callback(struct lws *wsi,
                                             enum lws_callback_reasons reason,
                                             void *user,
                                             void *in,
                                             size_t len)
        {
            HttpServer *srv = (HttpServer *)lws_context_user(lws_get_context(wsi));
            return srv->_impl->_lws_http_callback(wsi, reason, user, in, len);
        }

        static int _global_websocket_lws_callback(struct lws *wsi,
                                                  enum lws_callback_reasons reason,
                                                  void *user,
                                                  void *in,
                                                  size_t len)
        {
            HttpServer *srv = (HttpServer *)lws_context_user(lws_get_context(wsi));
            return srv->_impl->_lws_websocket_callback(wsi, reason, user, in, len);
        }

        // http callback
        int _lws_http_callback(struct lws *wsi,
                               enum lws_callback_reasons reason,
                               void *user,
                               void *in,
                               size_t len)
        {
            int rc = 0;
            HttpSession *session = (HttpSession *)user;
            if (session != nullptr)
            {
                session->wsi = wsi;
                session->in = in;
                session->len = len;
            }
            switch (reason)
            {
                case LWS_CALLBACK_HTTP:
                {
                    if (session == nullptr || session->len < 1)
                    {
                        lws_return_http_status(wsi, HTTP_STATUS_BAD_REQUEST, nullptr);
                        if (lws_http_transaction_completed(wsi))
                            rc = -1;
                        break;
                    }
                    session->new_request();
                    session->request_type = this->get_request_type(wsi);
                    rc = this->on_http_request(session, (char *)session->in);
                    if (session->should_complete_transaction)
                    {
                        if (lws_http_transaction_completed(wsi))
                            rc = -1;
                    }
                    break;
                }
                case LWS_CALLBACK_HTTP_BODY:
                {
                    if (session == nullptr)
                        break;
                    rc = this->on_http_body(session, (const uint8_t *)in, (size_t)len);
                    break;
                }
                case LWS_CALLBACK_HTTP_BODY_COMPLETION:
                {
                    if (session == nullptr)
                        break;
                    this->on_http_body_end(session);
                    if (lws_http_transaction_completed(wsi))
                        rc = -1;
                    break;
                }
                case LWS_CALLBACK_HTTP_FILE_COMPLETION:
                {
                    if (lws_http_transaction_completed(wsi))
                        rc = -1;
                    break;
                }
                case LWS_CALLBACK_HTTP_WRITEABLE:
                {
                    if (session != nullptr && session->stream_provider)
                    {
                        ArrByte chunk;
                        bool has_more = session->stream_provider(chunk);
                        if (chunk.size() > 0)
                        {
                            ArrByte buffer;
                            buffer.resize(chunk.size() + LWS_PRE);
                            memcpy(buffer.data() + LWS_PRE, chunk.data(), chunk.size());
                            auto write_proto = has_more ? LWS_WRITE_HTTP : LWS_WRITE_HTTP_FINAL;
                            if (lws_write(wsi,
                                          (u_char *)(buffer.data() + LWS_PRE),
                                          chunk.size(),
                                          (enum lws_write_protocol)write_proto)
                                < (int)chunk.size())
                            {
                                rc = -1;
                                break;
                            }
                        }
                        if (has_more)
                            lws_callback_on_writable(wsi);
                        else
                        {
                            session->stream_provider = nullptr;
                            [[maybe_unused]] int ret = lws_http_transaction_completed(wsi);
                            rc = -1;
                        }
                    }
                    break;
                }
                case LWS_CALLBACK_CLOSED_HTTP:
                {
                    break;
                }
                case LWS_CALLBACK_CHECK_ACCESS_RIGHTS:
                {
                    SIHD_LOG(debug, "HttpServer: Callback check access rights");
                    break;
                }
                case LWS_CALLBACK_PROCESS_HTML:
                {
                    SIHD_LOG(debug, "HttpServer: Callback check process HTML");
                    break;
                }
                case LWS_CALLBACK_ADD_HEADERS:
                {
                    if (!default_cors_origin.empty())
                    {
                        struct lws_process_html_args *args = (struct lws_process_html_args *)in;
                        if (lws_add_http_header_by_name(wsi,
                                                        (const unsigned char *)"access-control-allow-origin:",
                                                        (const unsigned char *)default_cors_origin.c_str(),
                                                        (int)default_cors_origin.size(),
                                                        (unsigned char **)&args->p,
                                                        (unsigned char *)args->p + args->max_len))
                        {
                            SIHD_LOG(error, "HttpServer: failed to add CORS header");
                            rc = 1;
                        }
                    }
                    break;
                }
                case LWS_CALLBACK_LOCK_POLL:
                case LWS_CALLBACK_UNLOCK_POLL:
                case LWS_CALLBACK_WSI_DESTROY:
                case LWS_CALLBACK_PROTOCOL_INIT:
                case LWS_CALLBACK_PROTOCOL_DESTROY:
                case LWS_CALLBACK_HTTP_BIND_PROTOCOL:
                case LWS_CALLBACK_HTTP_DROP_PROTOCOL:
                    break;
                case LWS_CALLBACK_FILTER_HTTP_CONNECTION:
                {
                    if (http_filter != nullptr)
                    {
                        std::string_view uri((const char *)in, len);

                        HttpFilterInfo info;
                        info.uri = uri;
                        info.method = this->get_request_type(wsi);
                        info.client_ip = this->get_client_ip(wsi);

                        static constexpr enum lws_token_indexes filter_headers[] = {
                            WSI_TOKEN_HOST,
                            WSI_TOKEN_HTTP_CONTENT_TYPE,
                            WSI_TOKEN_HTTP_USER_AGENT,
                            WSI_TOKEN_HTTP_ACCEPT,
                            WSI_TOKEN_ORIGIN,
                            WSI_TOKEN_HTTP_AUTHORIZATION,
                            WSI_TOKEN_HTTP_COOKIE,
                            WSI_TOKEN_HTTP_REFERER,
                        };
                        static constexpr const char *filter_header_names[] = {
                            "host",
                            "content-type",
                            "user-agent",
                            "accept",
                            "origin",
                            "authorization",
                            "cookie",
                            "referer",
                        };
                        for (size_t i = 0; i < sizeof(filter_headers) / sizeof(filter_headers[0]); ++i)
                        {
                            auto val = this->get_header(wsi, filter_headers[i]);
                            if (val.has_value() && !val->empty())
                                info.headers[filter_header_names[i]] = std::move(*val);
                        }

                        if (!http_filter->on_filter_connection(info))
                        {
                            lws_return_http_status(wsi, HTTP_STATUS_FORBIDDEN, nullptr);
                            rc = -1;
                        }
                    }
                    break;
                }
                case LWS_CALLBACK_VERIFY_BASIC_AUTHORIZATION:
                {
                    if (http_authenticator != nullptr)
                    {
                        std::string_view credentials((const char *)in, len);
                        auto sep = credentials.find(':');
                        if (sep != std::string_view::npos)
                        {
                            std::string_view user = credentials.substr(0, sep);
                            std::string_view pass = credentials.substr(sep + 1);
                            rc = http_authenticator->on_basic_auth(user, pass) ? 1 : 0;
                        }
                        else
                            rc = 0;
                    }
                    else
                        rc = 1;
                    break;
                }
                case LWS_CALLBACK_FILTER_NETWORK_CONNECTION:
                {
                    char client_name[128];
                    char client_ip[INET6_ADDRSTRLEN];
                    lws_sockfd_type fd = (size_t)in;
                    lws_get_peer_addresses(wsi,
                                           fd,
                                           client_name,
                                           sizeof(client_name),
                                           client_ip,
                                           sizeof(client_ip));
                    SIHD_LOG(debug,
                             "HttpServer: received client connect from {} ({})",
                             client_name,
                             client_ip);
                    rc = 0;
                    break;
                }
                case LWS_CALLBACK_SERVER_NEW_CLIENT_INSTANTIATED:
                {
                    break;
                }
                default:
                    break;
            }
            return rc;
        }

        HttpRequest::RequestType get_request_type(struct lws *wsi)
        {
            if (lws_hdr_total_length(wsi, WSI_TOKEN_GET_URI))
                return HttpRequest::Get;
            if (lws_hdr_total_length(wsi, WSI_TOKEN_POST_URI))
                return HttpRequest::Post;
            else if (lws_hdr_total_length(wsi, WSI_TOKEN_PUT_URI))
                return HttpRequest::Put;
            else if (lws_hdr_total_length(wsi, WSI_TOKEN_DELETE_URI))
                return HttpRequest::Delete;
            else if (lws_hdr_total_length(wsi, WSI_TOKEN_OPTIONS_URI))
                return HttpRequest::Options;
            return HttpRequest::None;
        }

        int on_http_request(HttpSession *session, std::string_view path)
        {
            SIHD_LOG(debug, "HttpServer: {} request: {}", HttpRequest::type_str(session->request_type), path);

            // CORS preflight
            if (session->request_type == HttpRequest::Options && !default_cors_origin.empty())
            {
                HttpResponse response;
                response.set_status(HttpStatus::NoContent);
                response.http_header().set_server(default_server_name);
                response.http_header().set_header("access-control-allow-origin:", default_cors_origin);
                response.http_header().set_header("access-control-allow-methods:",
                                                  "GET, POST, PUT, DELETE, OPTIONS");
                response.http_header().set_header("access-control-allow-headers:",
                                                  "content-type, authorization");
                response.http_header().set_header("access-control-max-age:", "86400");
                response.http_header().set_content_length(0);
                this->send_http_headers(session->wsi, response);
                return 0;
            }

            // Auth enforcement (Basic or Bearer) - per webservice, with server fallback
            {
                auto auth_header = this->get_header(session->wsi, WSI_TOKEN_HTTP_AUTHORIZATION);
                if (auth_header.has_value())
                    session->cached_auth_header = std::make_unique<std::string>(std::move(*auth_header));
            }

            int rc = 0;
            std::string resource_path;
            if (server->get_resource_path(path, resource_path))
            {
                std::string type
                    = fmt::format("{}; charset={}", mime.get(fs::extension(resource_path)), encoding);
                if (lws_serve_http_file(session->wsi, resource_path.c_str(), type.c_str(), nullptr, 0) < 0)
                    rc = -1;
                session->should_complete_transaction = false;
                return rc;
            }
            try
            {
                if (this->check_webservices(session, path))
                    return session->rc;
            }
            catch (const std::runtime_error & error)
            {
                SIHD_LOG(error, "HttpServer: {}", error.what());
                return -1;
            }

            if (page_404_path.empty())
                return this->send_404(session->wsi, "<html><body><h1>404 file not found</h1></body></html>")
                           ? 0
                           : -1;

            return rc;
        }

        int on_http_body(HttpSession *session, const uint8_t *buf, size_t size)
        {
            if (session->content != nullptr)
            {
                size_t current_offset = session->content_size;
                session->content->copy_from(buf, size, current_offset);
                session->content_size += size;
            }
            return 0;
        }

        int on_http_body_end(HttpSession *session)
        {
            int rc = 0;
            if (session->request != nullptr && session->content != nullptr)
            {
                session->request->set_content(*session->content);
                WebService *webservice = this->_get_webservice_from_path(session->request->url());
                if (webservice != nullptr)
                    rc = this->serve_webservice(session, webservice, *session->request);
                session->clear_request();
            }
            else
                lws_return_http_status(session->wsi, HTTP_STATUS_OK, nullptr);
            return rc;
        }

        WebService *_get_webservice_from_path(std::string_view path, std::string *webservice_name = nullptr)
        {
            std::string_view path_view(path);

            if (path_view.empty())
                return nullptr;
            if (path_view[0] == '/')
                path_view.remove_prefix(1);
            size_t first_slash_idx = path_view.find('/');
            if (first_slash_idx == std::string_view::npos)
                return nullptr;
            std::string name = std::string(path_view.substr(0, first_slash_idx));
            if (webservice_name != nullptr)
                *webservice_name = name;
            return server->get_child<WebService>(name);
        }

        bool check_webservices(HttpSession *session, std::string_view path)
        {
            std::string webservice_name;
            WebService *webservice = this->_get_webservice_from_path(path, &webservice_name);
            if (webservice == nullptr)
                return false;
            if (session->request_type == HttpRequest::Post || session->request_type == HttpRequest::Put)
            {
                auto content_length_header_opt
                    = this->get_header(session->wsi, WSI_TOKEN_HTTP_CONTENT_LENGTH);
                if (content_length_header_opt.has_value() == false)
                    throw std::runtime_error(
                        fmt::format("failed to copy content length header serving webservice: {}",
                                    webservice_name));

                std::string & content_length_header = content_length_header_opt.value();
                if (content_length_header.empty())
                    return false;
                size_t content_length_header_size;
                if (str::convert_from_string(content_length_header, content_length_header_size, 10) == false)
                    throw std::runtime_error("content length header has no number");

                if (content_length_header_size == 0)
                {
                    HttpRequest request
                        = HttpRequest(path, this->get_uri_args(session->wsi), session->request_type);
                    this->serve_webservice(session, webservice, request);
                }
                else
                {
                    session->content_size = 0;
                    session->content = std::make_unique<ArrUByte>();
                    session->request = std::make_unique<HttpRequest>(path,
                                                                     this->get_uri_args(session->wsi),
                                                                     session->request_type);
                    session->content->resize(content_length_header_size);
                    session->should_complete_transaction = false;
                }
            }
            else
            {
                HttpRequest request
                    = HttpRequest(path, this->get_uri_args(session->wsi), session->request_type);
                this->serve_webservice(session, webservice, request);
            }
            return true;
        }

        static std::unordered_map<std::string, std::string>
            parse_query_params(const std::vector<std::string> & uri_args)
        {
            std::unordered_map<std::string, std::string> params;
            for (const auto & arg : uri_args)
            {
                size_t eq = arg.find('=');
                if (eq != std::string::npos)
                    params[arg.substr(0, eq)] = arg.substr(eq + 1);
                else
                    params[arg] = "";
            }
            return params;
        }

        // Checks auth for the webservice. Sends 401 and returns false if unauthorized.
        bool check_and_apply_auth(HttpSession *session, WebService *webservice, HttpRequest & request)
        {
            IHttpAuthenticator *authenticator = this->get_authenticator_for(webservice);
            if (authenticator == nullptr)
                return true;

            std::string_view auth_header_value;
            if (session->cached_auth_header)
                auth_header_value = *session->cached_auth_header;

            AuthResult auth = this->parse_authorization(auth_header_value, authenticator);
            if (!auth.authorized)
            {
                HttpResponse response(&mime);
                response.set_status(HttpStatus::Unauthorized);
                response.http_header().set_server(default_server_name);
                response.http_header().set_header("www-authenticate:", "Basic realm=\"sihd\", Bearer");
                response.http_header().set_content_length(0);
                this->send_http_headers(session->wsi, response);
                return false;
            }
            if (!auth.user.empty())
                request.set_auth_user(auth.user);
            if (!auth.token.empty())
                request.set_auth_token(auth.token);
            return true;
        }

        // Sends the response to the client.
        // Returns true if streaming was initiated (subsequent writes via LWS_CALLBACK_HTTP_WRITEABLE).
        // Returns false if the full response was sent synchronously; sets session->rc on write error.
        bool send_response(HttpSession *session, HttpResponse & response)
        {
            if (response.is_streaming())
            {
                response.http_header().set_header("connection:", "close");
                this->send_http_headers(session->wsi, response);
                session->stream_provider = std::move(response.stream_provider());
                session->should_complete_transaction = false;
                lws_callback_on_writable(session->wsi);
                return true;
            }
            // HTTP/1.0 clients (e.g. ab) require explicit Connection: keep-alive in the
            // response to reuse the connection. Without it they assume the server will
            // close the connection (HTTP/1.0 default), while lws keeps it open → deadlock.
            char conn_hdr[32];
            if (lws_hdr_copy(session->wsi, conn_hdr, sizeof(conn_hdr), WSI_TOKEN_CONNECTION) > 0
                && str::iequals(conn_hdr, "keep-alive"))
            {
                response.http_header().set_header("connection:", "keep-alive");
            }
            const ArrByte & data = response.content();
            response.http_header().set_content_length(data.size());
            this->send_http_headers(session->wsi, response);
            if (data.size() > 0)
            {
                if (lws_write(session->wsi, (unsigned char *)data.buf(), data.size(), LWS_WRITE_HTTP_FINAL)
                    < (int)data.size())
                    session->rc = -1;
            }
            return false;
        }

        bool serve_webservice(HttpSession *session, WebService *webservice, HttpRequest & request)
        {
            std::string_view path_view = request.url();
            if (path_view.empty())
                return false;
            if (path_view[0] == '/')
                path_view.remove_prefix(1);
            size_t first_slash_idx = path_view.find('/');
            if (first_slash_idx == std::string_view::npos)
                return false;
            std::string rest_of_path(path_view.substr(first_slash_idx + 1));

            request.set_client_ip(this->get_client_ip(session->wsi));
            request.set_query_params(parse_query_params(request.uri_args()));

            if (!check_and_apply_auth(session, webservice, request))
                return true;

            HttpResponse response(&mime);
            response.http_header().set_server(default_server_name);
            if (!webservice->call(rest_of_path, request, response))
                return false;
            send_response(session, response);
            return true;
        }

        // websocket callback
        int _lws_websocket_callback(struct lws *wsi,
                                    enum lws_callback_reasons reason,
                                    void *user,
                                    void *in,
                                    size_t len)
        {
            int rc = 0;
            IWebsocketHandler *handler = nullptr;
            WebsocketSession *session = (WebsocketSession *)user;
            if (session != nullptr)
            {
                session->protocol = lws_get_protocol(wsi);
                session->wsi = wsi;
                session->in = in;
                session->len = len;
                handler = websocket_handler_lst[session->protocol->id];
            }
            switch (reason)
            {
                case LWS_CALLBACK_ESTABLISHED:
                {
                    if (handler != nullptr && session != nullptr)
                        handler->on_open(session->protocol->name);
                    break;
                }
                case LWS_CALLBACK_SERVER_WRITEABLE:
                {
                    if (handler != nullptr && session != nullptr)
                        rc = this->on_websocket_write(handler, session);
                    break;
                }
                case LWS_CALLBACK_RECEIVE:
                {
                    if (handler != nullptr && session != nullptr)
                    {
                        ArrChar arr;
                        arr.assign_bytes((uint8_t *)session->in, session->len);
                        rc = handler->on_read(arr) ? 0 : -1;
                        if (rc == 0)
                            lws_callback_on_writable(session->wsi);
                    }
                    break;
                }
                case LWS_CALLBACK_CLOSED:
                {
                    if (handler != nullptr)
                        handler->on_close();
                    break;
                }
                case LWS_CALLBACK_WS_PEER_INITIATED_CLOSE:
                {
                    if (handler != nullptr)
                    {
                        uint16_t close_code = 0;
                        std::string_view close_reason;
                        if (len >= 2)
                        {
                            close_code = (uint16_t)(((uint8_t *)in)[0] << 8 | ((uint8_t *)in)[1]);
                            if (len > 2)
                                close_reason = std::string_view((const char *)in + 2, len - 2);
                        }
                        handler->on_peer_close(close_code, close_reason);
                    }
                    break;
                }
                case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
                {
                    const char *error = (const char *)in;
                    if (error != nullptr)
                        SIHD_LOG(warning, "HttpServer: client connection error: {}", error);
                    break;
                }
                case LWS_CALLBACK_RECEIVE_PONG:
                {
                    SIHD_LOG(debug, "HttpServer: received pong callback");
                    break;
                }
#if defined(LWS_CALLBACK_CONNECTING)
                case LWS_CALLBACK_CONNECTING:
                {
                    lws_sockfd_type *fd = (lws_sockfd_type *)in;
                    SIHD_LOG(debug, "Callback connecting file descriptor: {}", fd);
                    break;
                }
#endif
                default:
                    break;
            }
            return rc;
        }

        int on_websocket_write(IWebsocketHandler *handler, WebsocketSession *session)
        {
            ArrChar arr_to_write;
            WriteProtocol lws_proto = WriteProtocol::Text;
            bool ret = handler->on_write(arr_to_write, lws_proto);
            if (ret && arr_to_write.size() > 0)
            {
                ArrByte buffer;
                buffer.resize(arr_to_write.size() + LWS_PRE);
                memcpy(buffer.data() + LWS_PRE, arr_to_write.data(), arr_to_write.size());
                int wrote = lws_write(session->wsi,
                                      (u_char *)(buffer.data() + LWS_PRE),
                                      arr_to_write.size(),
                                      (enum lws_write_protocol)write_protocol_to_lws(lws_proto));
                if (wrote < (int)arr_to_write.size())
                    ret = false;
            }
            return ret ? 0 : -1;
        }

        // header/response helpers
        std::optional<std::string> get_header(struct lws *wsi, enum lws_token_indexes idx)
        {
            char content_header[SIHD_HTTP_HEADER_VALUE_BUFSIZE + 1];
            ssize_t hdr_len = lws_hdr_copy(wsi, content_header, SIHD_HTTP_HEADER_VALUE_BUFSIZE, idx);
            if (hdr_len < 0)
                return {};
            return std::string(content_header, hdr_len);
        }

        std::string get_client_ip(struct lws *wsi)
        {
            char ip_buf[INET6_ADDRSTRLEN];
            ip_buf[0] = 0;
            lws_get_peer_simple(wsi, ip_buf, INET6_ADDRSTRLEN);
            return ip_buf;
        }

        std::vector<std::string> get_uri_args(struct lws *wsi)
        {
            if (wsi == nullptr)
                return {};
            std::vector<std::string> ret;
            char buf[SIHD_HTTP_URI_BUFSIZE + 1];
            int i = 0;
            while (lws_hdr_copy_fragment(wsi, buf, SIHD_HTTP_URI_BUFSIZE, WSI_TOKEN_HTTP_URI_ARGS, i) > 0)
            {
                ret.push_back(buf);
                ++i;
            }
            return ret;
        }

        bool send_404(struct lws *wsi, std::string_view html_404)
        {
            HttpResponse response;
            response.set_status(HttpStatus::NotFound);
            response.http_header().set_server(default_server_name);
            response.http_header().set_accept_charset(encoding);
            response.http_header().set_content_type(mime.get("html"), encoding);
            response.http_header().set_content_length(html_404.size());
            this->send_http_headers(wsi, response);
            return lws_write(wsi, (unsigned char *)html_404.data(), html_404.size(), LWS_WRITE_HTTP_FINAL)
                   == (int)html_404.size();
        }

        bool send_http_no_content(struct lws *wsi, int code)
        {
            HttpResponse response;
            response.set_status(code);
            response.http_header().set_server(default_server_name);
            response.http_header().set_accept_charset(encoding);
            response.http_header().set_content_type(mime.get("html"), encoding);
            response.http_header().set_content_length(0);
            return this->send_http_headers(wsi, response);
        }

        bool send_http_redirect(struct lws *wsi, std::string_view redirect_path, int code = 301)
        {
            HttpResponse response;
            response.set_status(code);
            response.http_header().set_server(default_server_name);
            response.http_header().set_accept_charset(encoding);
            response.http_header().set_content_type(mime.get("html"), encoding);
            response.http_header().set_content_length(0);
            response.http_header().set_header((const char *)lws_token_to_string(WSI_TOKEN_HTTP_LOCATION),
                                              redirect_path);
            return this->send_http_headers(wsi, response);
        }

        bool send_http_headers(struct lws *wsi, HttpResponse & response)
        {
            int rc;
            uint8_t header_buf[SIHD_HTTP_HEADERS_BUFSIZE];
            memset(header_buf, 0, SIHD_HTTP_HEADERS_BUFSIZE);
            u_char *ptr = header_buf;
            u_char *end = header_buf + SIHD_HTTP_HEADERS_BUFSIZE;

            HttpHeader & headers = response.http_header();

            rc = lws_add_http_header_status(wsi, response.status(), &ptr, end);
            if (rc)
            {
                SIHD_LOG(error, "HttpHeader: cannot set status");
                return false;
            }
            for (const auto & [name, value] : headers.headers())
            {
                rc = lws_add_http_header_by_name(wsi,
                                                 (u_char *)name.c_str(),
                                                 (u_char *)value.c_str(),
                                                 value.size(),
                                                 &ptr,
                                                 end);
                if (rc)
                {
                    SIHD_LOG(error, "HttpHeader: cannot set header '{}'", name);
                    return false;
                }
            }
            rc = lws_finalize_http_header(wsi, &ptr, end);
            if (rc)
            {
                SIHD_LOG(error, "HttpHeader: cannot finalize HTTP headers");
                return false;
            }
            *ptr = 0;
            size_t write_size = ptr - header_buf;
            if (lws_write(wsi, header_buf, write_size, LWS_WRITE_HTTP_HEADERS) != (int)write_size)
            {
                SIHD_LOG(error, "HttpServer: failed to write HTTP headers");
                return false;
            }
            return true;
        }

        // protocols
        bool add_protocol(const char *name,
                          lws_callback_function *callback,
                          size_t struct_size,
                          size_t tx_packet_size)
        {
            ++protocols_count;
            lws_protocols *old_ptr = lws_protocols_ptr;
            lws_protocols_ptr
                = (lws_protocols *)realloc(lws_protocols_ptr, sizeof(lws_protocols) * (protocols_count + 1));
            if (lws_protocols_ptr != nullptr)
            {
                lws_protocols *proto = &lws_protocols_ptr[protocols_count - 1];
                memset(proto, 0, sizeof(lws_protocols));
                memset(proto + 1, 0, sizeof(lws_protocols));
                proto->name = name;
                proto->callback = callback;
                proto->per_session_data_size = struct_size;
                proto->tx_packet_size = tx_packet_size;
                proto->id = protocols_count - 1;
                proto->user = server;
            }
            else
            {
                SIHD_LOG(error, "HttpServer: failed to add protocol: {}", name);
                lws_protocols_ptr = old_ptr;
                --protocols_count;
            }
            return lws_protocols_ptr != nullptr;
        }

        bool check_all_protocols()
        {
            // Start at 1: protocol 0 is HTTP, which uses per-session lws_callback_on_writable.
            // Websocket protocols (1+) need polling to trigger LWS_CALLBACK_SERVER_WRITEABLE.
            size_t i = 1;
            while (i < protocols_count)
            {
                lws_callback_on_writable_all_protocol(lws_context_ptr, &lws_protocols_ptr[i]);
                ++i;
            }
            return i > 1;
        }

        struct AuthResult
        {
                bool authorized = false;
                std::string user;
                std::string token;
        };

        AuthResult parse_authorization(std::string_view auth_header_value, IHttpAuthenticator *authenticator)
        {
            AuthResult result;
            if (authenticator == nullptr || auth_header_value.empty())
            {
                result.authorized = (authenticator == nullptr);
                return result;
            }

            constexpr std::string_view bearer_prefix = "Bearer ";
            constexpr std::string_view basic_prefix = "Basic ";
            if (auth_header_value.starts_with(bearer_prefix))
            {
                std::string_view token_view = auth_header_value.substr(bearer_prefix.size());
                result.authorized = authenticator->on_token_auth(token_view);
                if (result.authorized)
                    result.token = token_view;
            }
            else if (auth_header_value.starts_with(basic_prefix))
            {
                std::string_view b64 = auth_header_value.substr(basic_prefix.size());
                char decoded[256];
                int decoded_len
                    = lws_b64_decode_string(std::string(b64).c_str(), decoded, sizeof(decoded) - 1);
                if (decoded_len > 0)
                {
                    decoded[decoded_len] = '\0';
                    std::string_view credentials(decoded, decoded_len);
                    auto sep = credentials.find(':');
                    if (sep != std::string_view::npos)
                    {
                        std::string_view user = credentials.substr(0, sep);
                        std::string_view pass = credentials.substr(sep + 1);
                        result.authorized = authenticator->on_basic_auth(user, pass);
                        if (result.authorized)
                            result.user = user;
                    }
                }
            }
            return result;
        }

        IHttpAuthenticator *get_authenticator_for(WebService *webservice)
        {
            if (webservice->authenticator() != nullptr)
                return webservice->authenticator();
            return http_authenticator;
        }

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

        Mime mime;
        LwsPollingScheduler polling_scheduler;
        StepWorker stepworker;

        size_t service_thread_count = 1;
        std::vector<std::jthread> service_threads;
};

// HttpServer public methods

HttpServer::HttpServer(const std::string & name, sihd::util::Node *parent):
    sihd::util::Node(name, parent),
    _impl(std::make_unique<Impl>(this))
{
    this->add_conf("encoding", &HttpServer::set_encoding);
    this->add_conf("port", &HttpServer::set_port);
    this->add_conf("root_dir", &HttpServer::set_root_dir);
    this->add_conf("poll_frequency", &HttpServer::set_poll_frequency);
    this->add_conf("ssl_cert_path", &HttpServer::set_ssl_cert_path);
    this->add_conf("ssl_cert_key", &HttpServer::set_ssl_cert_key);
    this->add_conf("404_path", &HttpServer::set_404_path);
    this->add_conf("server_name", &HttpServer::set_server_name);
    this->add_conf("cors_origin", &HttpServer::set_cors_origin);
    this->add_conf("resource_path", &HttpServer::add_resource_path);
    this->add_conf("service_thread_count", &HttpServer::set_service_thread_count);
}

HttpServer::~HttpServer()
{
    if (this->is_running())
        this->stop();
}

bool HttpServer::set_encoding(std::string_view encoding)
{
    _impl->encoding = encoding;
    return true;
}

bool HttpServer::set_port(int port)
{
    _impl->port = port;
    return true;
}

bool HttpServer::set_root_dir(std::string_view root_dir)
{
    bool ret = fs::is_dir(root_dir);
    if (ret)
        _impl->root_dir = fs::realpath(root_dir);
    else
        SIHD_LOG(error, "HttpServer: root dir does not exists: {}", root_dir);
    return ret;
}

bool HttpServer::set_poll_frequency(double freq)
{
    return _impl->stepworker.set_frequency(freq);
}

bool HttpServer::set_ssl_cert_path(std::string_view path)
{
    bool ret = fs::is_file(path);
    if (ret)
        _impl->ssl_cert_path = path;
    else
        SIHD_LOG(error, "HttpServer: ssl cert path does not exists: {}", path);
    return ret;
}

bool HttpServer::set_ssl_cert_key(std::string_view path)
{
    bool ret = fs::is_file(path);
    if (ret)
        _impl->ssl_cert_key = path;
    else
        SIHD_LOG(error, "HttpServer: ssl cert key does not exists: {}", path);
    return ret;
}

bool HttpServer::add_resource_path(const std::string & path)
{
    _impl->resources_path.insert(path);
    return true;
}

bool HttpServer::remove_resource_path(const std::string & path)
{
    auto it = _impl->resources_path.find(path);
    if (it != _impl->resources_path.end())
        _impl->resources_path.erase(it);
    return true;
}

bool HttpServer::set_404_path(std::string_view path)
{
    _impl->page_404_path = path;
    return true;
}

bool HttpServer::set_server_name(std::string_view name)
{
    _impl->default_server_name = name;
    return true;
}

bool HttpServer::set_cors_origin(std::string_view origin)
{
    _impl->default_cors_origin = origin;
    return true;
}

void HttpServer::set_http_filter(IHttpFilter *filter)
{
    _impl->http_filter = filter;
}

void HttpServer::set_authenticator(IHttpAuthenticator *authenticator)
{
    _impl->http_authenticator = authenticator;
}

bool HttpServer::set_service_thread_count(size_t count)
{
    _impl->service_thread_count = count < 1 ? 1 : count;
    return true;
}

void HttpServer::request_stop()
{
    _impl->stop = true;
}

bool HttpServer::on_stop()
{
    _impl->stop = true;
    {
        std::lock_guard l(_impl->mutex);
        if (_impl->lws_context_ptr != nullptr)
            lws_cancel_service(_impl->lws_context_ptr);
    }
    return true;
}

bool HttpServer::on_start()
{
    _impl->stop = false;

    struct lws_context_creation_info lws_info;
    memset(&lws_info, 0, sizeof(lws_context_creation_info));
    lws_info.port = _impl->port;
    lws_info.iface = nullptr;
    lws_info.protocols = _impl->lws_protocols_ptr;
    lws_info.gid = -1;
    lws_info.uid = -1;
    lws_info.user = this;
    lws_info.count_threads = _impl->service_thread_count;
    if (_impl->page_404_path.empty() == false)
        lws_info.error_document_404 = _impl->page_404_path.c_str();
    if (_impl->lws_mount_ptr != nullptr)
        lws_info.mounts = _impl->lws_mount_ptr;
    if (_impl->ssl_cert_path.empty() == false && _impl->ssl_cert_key.empty() == false)
    {
        lws_info.ssl_cert_filepath = _impl->ssl_cert_path.c_str();
        lws_info.ssl_private_key_filepath = _impl->ssl_cert_key.c_str();
        lws_info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    }

    _impl->lws_context_ptr = lws_create_context(&lws_info);
    if (_impl->lws_context_ptr == nullptr)
    {
        SIHD_LOG(error, "HttpServer: failed to create context");
        return false;
    }

    Defer defer_context([this] {
        std::lock_guard l(_impl->mutex);
        if (_impl->lws_context_ptr != nullptr)
        {
            lws_context_destroy(_impl->lws_context_ptr);
            _impl->lws_context_ptr = nullptr;
        }
    });

    _impl->port = lws_info.port;

    if (_impl->stepworker.start_sync_worker(this->name() + "-callback") == false)
        return false;

    for (size_t tsi = 1; tsi < _impl->service_thread_count; ++tsi)
    {
        _impl->service_threads.emplace_back([this, tsi] {
            while (!_impl->stop)
                lws_service_tsi(_impl->lws_context_ptr, 0, (int)tsi);
        });
    }

    this->service_set_ready();

    while (!_impl->stop)
        lws_service_tsi(_impl->lws_context_ptr, 0, 0);

    _impl->service_threads.clear();
    _impl->stepworker.stop_worker();
    return true;
}

bool HttpServer::get_resource_path(std::string_view path, std::string & res)
{
    if (path.size() > 0 && path[0] == '/')
        path = path.substr(1);

    if (!_impl->root_dir.empty())
    {
        std::string full_path = fs::combine(_impl->root_dir, path);
        if (fs::is_file(full_path))
        {
            res = std::move(full_path);
            return true;
        }
        if (path.empty() || fs::is_dir(full_path))
        {
            std::string index_path = fs::combine(full_path, "index.html");
            if (fs::is_file(index_path))
            {
                res = std::move(index_path);
                return true;
            }
        }
    }

    for (const std::string & resource_path : _impl->resources_path)
    {
        std::string tmp_path = fs::combine(resource_path, path);
        if (fs::is_file(tmp_path))
        {
            res = std::move(tmp_path);
            return true;
        }
    }

    return false;
}

bool HttpServer::add_websocket(const char *name, IWebsocketHandler *handler, size_t tx_packet_size)
{
    bool ret = _impl->add_protocol(name,
                                   Impl::_global_websocket_lws_callback,
                                   sizeof(Impl::WebsocketSession),
                                   tx_packet_size);
    if (ret)
    {
        _impl->websocket_handler_lst.resize(_impl->protocols_count);
        _impl->websocket_handler_lst[_impl->protocols_count - 1] = handler;
    }
    return ret;
}

} // namespace sihd::http

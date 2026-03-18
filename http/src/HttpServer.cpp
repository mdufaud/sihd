#include <libwebsockets.h>

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
            polling_scheduler(server)
        {
            stepworker.set_runnable(&polling_scheduler);
            stepworker.set_frequency(10);

            http_header_array.resize(SIHD_HTTP_HEADERS_BUFSIZE);
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
                    SIHD_LOG(debug, "HttpServer: Callback add header callback");
                    break;
                }
                case LWS_CALLBACK_LOCK_POLL:
                case LWS_CALLBACK_UNLOCK_POLL:
                case LWS_CALLBACK_WSI_DESTROY:
                    break;
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
                    SIHD_LOG(debug, "HttpServer: received client connect from {} ({})", client_name, client_ip);
                    rc = 0;
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
            return HttpRequest::None;
        }

        int on_http_request(HttpSession *session, std::string_view path)
        {
            SIHD_LOG(debug, "HttpServer: {} request: {}", HttpRequest::type_str(session->request_type), path);
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
                    throw std::runtime_error(fmt::format(
                        "failed to copy content length header serving webservice: {}", webservice_name));

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
                    session->request = std::make_unique<HttpRequest>(
                        path, this->get_uri_args(session->wsi), session->request_type);
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

            std::string ip = this->get_client_ip(session->wsi);
            request.set_client_ip(ip);

            HttpResponse response(&mime);
            response.http_header().set_server(default_server_name);
            if (webservice->call(rest_of_path, request, response))
            {
                const ArrByte & data = response.content();
                response.http_header().set_content_length(data.size());

                this->send_http_headers(session->wsi, response);
                if (data.size() > 0)
                {
                    if (lws_write_http(session->wsi, data.buf(), data.size()) < 0)
                        session->rc = -1;
                }
                return true;
            }
            return false;
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
                    }
                    break;
                }
                case LWS_CALLBACK_CLOSED:
                {
                    if (handler != nullptr)
                        handler->on_close();
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
            return lws_write_http(wsi, html_404.data(), html_404.size()) == (int)html_404.size();
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
            response.http_header().set_header(
                (const char *)lws_token_to_string(WSI_TOKEN_HTTP_LOCATION), redirect_path);
            return this->send_http_headers(wsi, response);
        }

        bool send_http_headers(struct lws *wsi, HttpResponse & response)
        {
            int rc;
            u_char *ptr = (u_char *)http_header_array.buf();
            u_char *end = (u_char *)http_header_array.buf() + http_header_array.size();

            HttpHeader & headers = response.http_header();

            rc = lws_add_http_header_status(wsi, response.status(), &ptr, end);
            if (rc)
            {
                SIHD_LOG(error, "HttpHeader: cannot set status");
                return false;
            }
            for (const auto & [name, value] : headers.headers())
            {
                rc = lws_add_http_header_by_name(
                    wsi, (u_char *)name.c_str(), (u_char *)value.c_str(), value.size(), &ptr, end);
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
            size_t write_size = ptr - http_header_array.buf();
            if (lws_write(wsi, http_header_array.buf(), write_size, LWS_WRITE_HTTP_HEADERS)
                != (int)write_size)
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
            size_t i = 0;
            while (i < protocols_count)
            {
                lws_callback_on_writable_all_protocol(lws_context_ptr, &lws_protocols_ptr[i]);
                ++i;
            }
            return i != 0;
        }

        // members
        HttpServer *server;

        std::atomic<bool> stop;
        int port;
        std::string root_dir;
        std::string old_dir;
        std::set<std::string> resources_path;
        std::string ssl_cert_path;
        std::string ssl_cert_key;
        std::mutex mutex;

        lws_http_mount *lws_mount_ptr;
        lws_context *lws_context_ptr;
        size_t protocols_count;
        lws_protocols *lws_protocols_ptr;

        std::vector<IWebsocketHandler *> websocket_handler_lst;
        std::string encoding;
        std::string default_server_name;
        std::string default_cors_origin;

        ArrUByte http_header_array;

        std::string page_404_path;

        Mime mime;
        LwsPollingScheduler polling_scheduler;
        StepWorker stepworker;
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
    this->add_conf("resource_path", &HttpServer::add_resource_path);
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
        _impl->root_dir = root_dir;
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
    if (!_impl->root_dir.empty())
    {
        _impl->old_dir = fs::cwd();
        if (!fs::chdir(_impl->root_dir))
        {
            SIHD_LOG(error, "HttpServer: could not change directory to: {}", _impl->root_dir);
            return false;
        }
    }

    _impl->stop = false;

    Defer put_old_dir_back([this] {
        if (!_impl->old_dir.empty())
        {
            if (!fs::chdir(_impl->old_dir))
            {
                SIHD_LOG(error, "HttpServer: could not return to directory: {}", _impl->old_dir);
            }
            _impl->old_dir.clear();
        }
    });

    struct lws_context_creation_info lws_info;
    memset(&lws_info, 0, sizeof(lws_context_creation_info));
    lws_info.port = _impl->port;
    lws_info.iface = nullptr;
    lws_info.protocols = _impl->lws_protocols_ptr;
    lws_info.gid = -1;
    lws_info.uid = -1;
    lws_info.user = this;
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

    this->service_set_ready();

    int n = 0;
    while (_impl->stop == false && n >= 0)
        n = lws_service(_impl->lws_context_ptr, 0);

    _impl->stepworker.stop_worker();
    return true;
}

bool HttpServer::get_resource_path(std::string_view path, std::string & res)
{
    if (path.size() > 0 && path[0] == '/')
        path = path.substr(1);

    if (!path.empty() && fs::is_file(path))
    {
        res = std::string(path);
        return true;
    }

    if (path.empty() || fs::is_dir(path))
    {
        res = fs::combine(path, "index.html");
        return true;
    }

    for (const std::string & resource_path : _impl->resources_path)
    {
        std::string tmp_path = fs::combine(path, resource_path);
        if (fs::exists(tmp_path))
        {
            res = std::move(tmp_path);
            return true;
        }
    }

    return false;
}

bool HttpServer::add_websocket(const char *name, IWebsocketHandler *handler, size_t tx_packet_size)
{
    bool ret = _impl->add_protocol(
        name, Impl::_global_websocket_lws_callback, sizeof(Impl::WebsocketSession), tx_packet_size);
    if (ret)
    {
        _impl->websocket_handler_lst.resize(_impl->protocols_count);
        _impl->websocket_handler_lst[_impl->protocols_count - 1] = handler;
    }
    return ret;
}

} // namespace sihd::http

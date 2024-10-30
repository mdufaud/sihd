#include <libwebsockets.h>

#include <sihd/util/Defer.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/fs.hpp>

#include <sihd/http/HttpServer.hpp>

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

SIHD_UTIL_REGISTER_FACTORY(HttpServer)

SIHD_NEW_LOGGER("sihd::http");

using namespace sihd::util;

HttpServer::HttpServer(const std::string & name, sihd::util::Node *parent):
    sihd::util::Node(name, parent),
    _stop(false),
    _port(0),
    _lws_mount_ptr(nullptr),
    _lws_context_ptr(nullptr),
    _lws_protocols_ptr(nullptr),
    _polling_scheduler(this)
{
    _worker.set_runnable(&_polling_scheduler);
    _worker.set_frequency(10);

    _http_header_array.resize(SIHD_HTTP_HEADERS_BUFSIZE);
    _http_response.http_header().set_server(this->name());
    _http_response.http_header().set_header(lws_token_to_string(WSI_TOKEN_HTTP_ACCESS_CONTROL_ALLOW_ORIGIN), "*");
    this->set_encoding("utf-8");

    _protocols_count = 0;
    this->add_protocol("http-server", HttpServer::_global_http_lws_callback, sizeof(HttpSession), 0);

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
    {
        std::lock_guard l(_mutex);
        if (_lws_protocols_ptr != nullptr)
        {
            free(_lws_protocols_ptr);
            _lws_protocols_ptr = nullptr;
        }
        if (_lws_context_ptr != nullptr)
        {
            lws_context_destroy(_lws_context_ptr);
            _lws_context_ptr = nullptr;
        }
    }
}

bool HttpServer::set_encoding(std::string_view encoding)
{
    _http_response.http_header().set_accept_charset(encoding);
    _encoding = encoding;
    return true;
}

bool HttpServer::set_port(int port)
{
    _port = port;
    return true;
}

bool HttpServer::set_root_dir(std::string_view root_dir)
{
    bool ret = fs::is_dir(root_dir);
    if (ret)
        _root_dir = root_dir;
    else
        SIHD_LOG(error, "HttpServer: root dir does not exists: {}", root_dir);
    return ret;
}

bool HttpServer::set_poll_frequency(double freq)
{
    return _worker.set_frequency(freq);
}

bool HttpServer::set_ssl_cert_path(std::string_view path)
{
    bool ret = fs::is_file(path);
    if (ret)
        _ssl_cert_path = path;
    else
        SIHD_LOG(error, "HttpServer: ssl cert path does not exists: {}", path);
    return ret;
}

bool HttpServer::set_ssl_cert_key(std::string_view path)
{
    bool ret = fs::is_file(path);
    if (ret)
        _ssl_cert_key = path;
    else
        SIHD_LOG(error, "HttpServer: ssl cert key does not exists: {}", path);
    return ret;
}

bool HttpServer::add_resource_path(const std::string & path)
{
    _resources_path.insert(path);
    return true;
}

bool HttpServer::remove_resource_path(const std::string & path)
{
    auto it = _resources_path.find(path);
    if (it != _resources_path.end())
        _resources_path.erase(it);
    return true;
}

bool HttpServer::set_404_path(std::string_view path)
{
    _404_page_path = path;
    return true;
}

bool HttpServer::set_server_name(std::string_view name)
{
    _http_response.http_header().set_server(name);
    return true;
}

void HttpServer::request_stop()
{
    _stop = true;
}

bool HttpServer::on_stop()
{
    _stop = true;
    {
        std::lock_guard l(_mutex);
        if (_lws_context_ptr != nullptr)
            lws_cancel_service(_lws_context_ptr);
    }
    return true;
}

bool HttpServer::on_start()
{
    if (!_root_dir.empty())
    {
        _old_dir = fs::cwd();
        if (!fs::chdir(_root_dir))
        {
            SIHD_LOG(error, "HttpServer: could not change directory to: {}", _root_dir);
            return false;
        }
    }

    _stop = false;

    Defer put_old_dir_back([this] {
        if (!_old_dir.empty())
        {
            if (!fs::chdir(_old_dir))
            {
                SIHD_LOG(error, "HttpServer: could not return to directory: {}", _old_dir);
            }
            _old_dir.clear();
        }
    });

    struct lws_context_creation_info lws_info;
    memset(&lws_info, 0, sizeof(lws_context_creation_info));
    lws_info.port = _port;
    lws_info.iface = nullptr;
    lws_info.protocols = _lws_protocols_ptr;
    lws_info.gid = -1;
    lws_info.uid = -1;
    lws_info.user = this;
    if (_404_page_path.empty() == false)
        lws_info.error_document_404 = _404_page_path.c_str();
    if (_lws_mount_ptr != nullptr)
        lws_info.mounts = _lws_mount_ptr;
    if (_ssl_cert_path.empty() == false && _ssl_cert_key.empty() == false)
    {
        lws_info.ssl_cert_filepath = _ssl_cert_path.c_str();
        lws_info.ssl_private_key_filepath = _ssl_cert_key.c_str();
        lws_info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    }

    _lws_context_ptr = lws_create_context(&lws_info);
    if (_lws_context_ptr == nullptr)
    {
        SIHD_LOG(error, "HttpServer: failed to create context");
        return false;
    }

    Defer defer_context([this] {
        std::lock_guard l(_mutex);
        if (_lws_context_ptr != nullptr)
        {
            lws_context_destroy(_lws_context_ptr);
            _lws_context_ptr = nullptr;
        }
    });

    // update port if _port set to 0
    _port = lws_info.port;

    if (_worker.start_sync_worker(this->name() + "-callback") == false)
        return false;

    int n = 0;
    while (_stop == false && n >= 0)
        n = lws_service(_lws_context_ptr, 0);

    _worker.stop_worker();
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

    for (const std::string & resource_path : _resources_path)
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

bool HttpServer::check_all_protocols()
{
    size_t i = 0;
    while (i < _protocols_count)
    {
        lws_callback_on_writable_all_protocol(_lws_context_ptr, &_lws_protocols_ptr[i]);
        ++i;
    }
    return i != 0;
}

int HttpServer::_global_http_lws_callback(struct lws *wsi,
                                          enum lws_callback_reasons reason,
                                          void *user,
                                          void *in,
                                          size_t len)
{
    HttpServer *server = (HttpServer *)lws_context_user(lws_get_context(wsi));
    return server->_lws_http_callback(wsi, reason, user, in, len);
}

int HttpServer::_lws_http_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
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
        // an http request has come from a client that is not asking to upgrade the connection to a websocket one.
        // this is a chance to serve http content
        case LWS_CALLBACK_HTTP:
        {
            if (session->len < 1)
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
            // the next `len` bytes data from the http request body HTTP connection is now available in `in`
            rc = this->on_http_body(session, (const uint8_t *)in, (size_t)len);
            break;
        }
        case LWS_CALLBACK_HTTP_BODY_COMPLETION:
        {
            // the expected amount of http request body has been delivered
            this->on_http_body_end(session);
            if (lws_http_transaction_completed(wsi))
                rc = -1;
            break;
        }
        case LWS_CALLBACK_HTTP_FILE_COMPLETION:
        {
            // a file requested to be sent down http link has completed
            rc = this->on_http_file_completion_end();
            if (lws_http_transaction_completed(wsi))
                rc = -1;
            break;
        }
        case LWS_CALLBACK_CLOSED_HTTP:
        {
            // when a HTTP (non-websocket) session ends
            rc = this->on_http_request_end();
            break;
        }
        case LWS_CALLBACK_CHECK_ACCESS_RIGHTS:
        {
            // This gives the user code a chance to forbid an http access
            SIHD_LOG(debug, "HttpServer: Callback check access rights");
            struct lws_process_html_args *args = (struct lws_process_html_args *)in;
            (void)args;
            break;
        }
        case LWS_CALLBACK_PROCESS_HTML:
        {
            // This gives your user code a chance to mangle outgoing HTML
            SIHD_LOG(debug, "HttpServer: Callback check process HTML");
            struct lws_process_html_args *args = (struct lws_process_html_args *)in;
            (void)args;
            break;
        }
        case LWS_CALLBACK_ADD_HEADERS:
        {
            // This gives your user code a chance to add headers to a server transaction bound to your protocol
            SIHD_LOG(debug, "HttpServer: Callback add header callback");
            struct lws_process_html_args *args = (struct lws_process_html_args *)in;
            (void)args;
            /*
            if (lws_add_http_header_by_name(wsi,
                    (unsigned char *)"set-cookie:",
                    (unsigned char *)cookie, cookie_len,
                    (unsigned char **)&args->p,
                    (unsigned char *)args->p + args->max_len))
                rc = 1;
            */
            break;
        }
        case LWS_CALLBACK_LOCK_POLL:
        {
            /*
             * lock mutex to protect pollfd state
             * called before any other POLL related callback
             */
            break;
        }
        case LWS_CALLBACK_UNLOCK_POLL:
        {
            /*
             * unlock mutex to protect pollfd state when
             * called after any other POLL related callback
             */
            break;
        }
        case LWS_CALLBACK_WSI_DESTROY:
        {
            // called multiple times for a single client
            break;
        }
        case LWS_CALLBACK_FILTER_NETWORK_CONNECTION:
        {
            char client_name[128];
            char client_ip[INET6_ADDRSTRLEN];
            lws_get_peer_addresses(wsi, (int)(long)in, client_name, sizeof(client_name), client_ip, sizeof(client_ip));
            SIHD_LOG(debug, "HttpServer: received client connect from {} ({})", client_name, client_ip);
            // send non 0 to refuse connection
            rc = 0;
            break;
        }
        default:
            break;
    }
    return rc;
}

HttpRequest::RequestType HttpServer::get_request_type(struct lws *wsi)
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

int HttpServer::on_http_request(HttpSession *session, std::string_view path)
{
    SIHD_LOG(debug, "HttpServer: {} request: {}", HttpRequest::type_str(session->request_type), path);
    int rc = 0;
    std::string resource_path;
    if (this->get_resource_path(path, resource_path))
    {
        std::string type = fmt::format("{}; charset={}", _mime.get(fs::extension(resource_path)), _encoding);
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

    if (_404_page_path.empty())
        return this->send_404(session->wsi, "<html><body><h1>404 file not found</h1></body></html>") ? 0 : -1;

    return rc;
}

int HttpServer::on_http_body(HttpSession *session, const uint8_t *buf, size_t size)
{
    if (session->content != nullptr)
    {
        // expected a body from a request
        size_t current_offset = session->content_size;
        session->content->copy_from(buf, size, current_offset);
        session->content_size += size;
    }
    return 0;
}

int HttpServer::on_http_body_end(HttpSession *session)
{
    int rc = 0;
    if (session->request != nullptr && session->content != nullptr)
    {
        // end of expected body
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

int HttpServer::on_http_request_end()
{
    return 0;
}

int HttpServer::on_http_file_completion_end()
{
    return 0;
}

WebService *HttpServer::_get_webservice_from_path(std::string_view path, std::string *webservice_name)
{
    std::string_view path_view(path);

    if (path_view[0] == '/')
        path_view.remove_prefix(1);
    size_t first_slash_idx = path_view.find('/');
    if (first_slash_idx == std::string_view::npos)
        return nullptr;
    std::string name = std::string(path_view.substr(0, first_slash_idx));
    if (webservice_name != nullptr)
        *webservice_name = name;
    return this->get_child<WebService>(name);
}

std::optional<std::string> HttpServer::get_header(struct lws *wsi, enum lws_token_indexes idx)
{
    char content_header[SIHD_HTTP_HEADER_VALUE_BUFSIZE + 1];
    ssize_t hdr_len = lws_hdr_copy(wsi, content_header, SIHD_HTTP_HEADER_VALUE_BUFSIZE, idx);
    if (hdr_len < 0)
        return {};
    return std::string(content_header, hdr_len);
}

bool HttpServer::check_webservices(HttpSession *session, std::string_view path)
{
    std::string webservice_name;
    WebService *webservice = this->_get_webservice_from_path(path, &webservice_name);
    if (webservice == nullptr)
        return false;
    if (session->request_type == HttpRequest::Post || session->request_type == HttpRequest::Put)
    {
        // check if there will be only header or request body
        auto content_length_header_opt = this->get_header(session->wsi, WSI_TOKEN_HTTP_CONTENT_LENGTH);
        if (content_length_header_opt.has_value() == false)
            throw std::runtime_error(
                fmt::format("failed to copy content length header serving webservice: {}", webservice_name));

        std::string & content_length_header = content_length_header_opt.value();
        if (content_length_header.empty())
            return false;
        size_t content_length_header_size;
        if (util::str::convert_from_string(content_length_header, content_length_header_size, 10) == false)
            throw std::runtime_error("content length header has no number");

        if (content_length_header_size == 0)
        {
            HttpRequest request = HttpRequest(path, this->get_uri_args(session->wsi), session->request_type);
            // no 'body' coming - call webservice now
            this->serve_webservice(session, webservice, request);
        }
        else
        {
            // must wait for http body callback completion - setuping it
            session->content_size = 0;
            session->content = new ArrUByte();
            session->request = new HttpRequest(path, this->get_uri_args(session->wsi), session->request_type);
            if (session->content == nullptr || session->request == nullptr)
            {
                session->clear_request();
                throw std::runtime_error(
                    fmt::format("failed to allocate session in POST or PUT request serving webservice: {}",
                                webservice_name));
            }
            session->content->resize(content_length_header_size);
            // do not complete transaction - we are waiting for the request's body
            session->should_complete_transaction = false;
        }
    }
    else
    {
        HttpRequest request = HttpRequest(path, this->get_uri_args(session->wsi), session->request_type);
        // HttpRequest is GET or DELETE
        this->serve_webservice(session, webservice, request);
    }
    return true;
}

bool HttpServer::serve_webservice(HttpSession *session, WebService *webservice, HttpRequest & request)
{
    std::string_view path_view = request.url();
    if (path_view[0] == '/')
        path_view.remove_prefix(1);
    size_t first_slash_idx = path_view.find('/');
    if (first_slash_idx == std::string_view::npos)
        return false;
    std::string rest_of_path(path_view.substr(first_slash_idx + 1));

    std::string ip = this->get_client_ip(session->wsi);
    request.set_client_ip(ip);

    HttpResponse response(&_mime);
    response.http_header().set_server(_http_response.http_header().server());
    if (webservice->call(rest_of_path, request, response))
    {
        // webservice called
        const ArrByte & data = response.content();
        response.http_header().set_content_length(data.size());

        this->send_http_headers(session->wsi, response);
        if (data.size() > 0)
        {
            // write body if there is
            if (lws_write_http(session->wsi, data.buf(), data.size()) < 0)
                session->rc = -1;
        }
        return true;
    }
    return false;
}

int HttpServer::_global_websocket_lws_callback(struct lws *wsi,
                                               enum lws_callback_reasons reason,
                                               void *user,
                                               void *in,
                                               size_t len)
{
    HttpServer *server = (HttpServer *)lws_context_user(lws_get_context(wsi));
    return server->_lws_websocket_callback(wsi, reason, user, in, len);
}

int HttpServer::_lws_websocket_callback(struct lws *wsi,
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
        handler = _websocket_handler_lst[session->protocol->id];
    }
    switch (reason)
    {
        case LWS_CALLBACK_ESTABLISHED:
        {
            rc = this->on_websocket_open(handler, session);
            break;
        }
        case LWS_CALLBACK_SERVER_WRITEABLE:
        {
            rc = this->on_websocket_write(handler, session);
            break;
        }
        case LWS_CALLBACK_RECEIVE:
        {
            rc = this->on_websocket_read(handler, session);
            break;
        }
        case LWS_CALLBACK_CLOSED:
        {
            rc = this->on_websocket_close(handler, session);
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
        // Called before a socketfd is about to connect()
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

int HttpServer::on_websocket_open(IWebsocketHandler *handler, WebsocketSession *session)
{
    handler->on_open(session->protocol->name);
    return 0;
}

int HttpServer::on_websocket_read(IWebsocketHandler *handler, WebsocketSession *session)
{
    sihd::util::ArrChar arr;
    arr.assign_bytes((uint8_t *)session->in, session->len);
    return handler->on_read(arr) ? 0 : -1;
}

int HttpServer::on_websocket_write(IWebsocketHandler *handler, WebsocketSession *session)
{
    sihd::util::ArrChar arr_to_write;
    LwsWriteProtocol lws_proto;
    lws_proto.set_txt();
    bool ret = handler->on_write(arr_to_write, lws_proto);
    if (ret && arr_to_write.size() > 0)
    {
        sihd::util::ArrByte buffer;
        buffer.resize(arr_to_write.size() + LWS_PRE);
        memcpy(buffer.data() + LWS_PRE, arr_to_write.data(), arr_to_write.size());
        int wrote = lws_write(session->wsi,
                              (u_char *)(buffer.data() + LWS_PRE),
                              arr_to_write.size(),
                              (enum lws_write_protocol)lws_proto.protocol);
        if (wrote < (int)arr_to_write.size())
            ret = false;
    }
    return ret ? 0 : -1;
}

int HttpServer::on_websocket_close(IWebsocketHandler *handler, WebsocketSession *session)
{
    (void)session;
    handler->on_close();
    return 0;
}

std::string HttpServer::get_client_ip(struct lws *wsi)
{
    char ip_buf[INET6_ADDRSTRLEN];
    ip_buf[0] = 0;
    lws_get_peer_simple(wsi, ip_buf, INET6_ADDRSTRLEN);
    return ip_buf;
}

std::vector<std::string> HttpServer::get_uri_args(struct lws *wsi)
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

bool HttpServer::send_404(struct lws *wsi, std::string_view html_404)
{
    _http_response.set_status(HTTP_STATUS_NOT_FOUND);
    _http_response.http_header().set_content_type(_mime.get("html"), _encoding);
    _http_response.http_header().set_content_length(html_404.size());
    this->send_http_headers(wsi, _http_response);
    return lws_write_http(wsi, html_404.data(), html_404.size()) == (int)html_404.size();
}

bool HttpServer::send_http_no_content(struct lws *wsi, int code)
{
    _http_response.set_status(code);
    _http_response.http_header().set_content_type(_mime.get("html"), _encoding);
    _http_response.http_header().set_content_length(0);
    return this->send_http_headers(wsi, _http_response);
}

bool HttpServer::send_http_redirect(struct lws *wsi, std::string_view redirect_path, int code)
{
    _http_response.set_status(code);
    _http_response.http_header().set_content_type(_mime.get("html"), _encoding);
    _http_response.http_header().set_content_length(0);
    _http_response.http_header().set_header(lws_token_to_string(WSI_TOKEN_HTTP_LOCATION), redirect_path);
    bool ret = this->send_http_headers(wsi, _http_response);
    _http_response.http_header().remove_header(lws_token_to_string(WSI_TOKEN_HTTP_LOCATION));
    return ret;
}

bool HttpServer::send_http_headers(struct lws *wsi, HttpResponse & response)
{
    int rc;
    u_char *ptr = (u_char *)_http_header_array.buf();
    u_char *end = (u_char *)_http_header_array.buf() + _http_header_array.size();

    HttpHeader & headers = response.http_header();

    // STATUS
    rc = lws_add_http_header_status(wsi, response.status(), &ptr, end);
    if (rc)
        SIHD_LOG(error, "HttpHeader: cannot set status");
    // HEADERS
    for (const auto & [name, value] : headers.headers())
    {
        rc = lws_add_http_header_by_name(wsi, (u_char *)name.c_str(), (u_char *)value.c_str(), value.size(), &ptr, end);
        if (rc)
            SIHD_LOG(error, "HttpHeader: cannot set header '{}'", name);
    }
    // FINALIZE
    rc = lws_finalize_http_header(wsi, &ptr, end);
    if (rc)
        SIHD_LOG(error, "HttpHeader: cannot finalize HTTP headers");
    *ptr = 0;
    // calculate header size
    size_t write_size = ptr - _http_header_array.buf();
    if (rc != 0)
        return false;
    if (lws_write(wsi, _http_header_array.buf(), write_size, LWS_WRITE_HTTP_HEADERS) != (int)write_size)
    {
        SIHD_LOG(error, "HttpServer: failed to write HTTP headers");
        return false;
    }
    return true;
}

bool HttpServer::add_protocol(const char *name,
                              lws_callback_function *callback,
                              size_t struct_size,
                              size_t tx_packet_size)
{
    ++_protocols_count;
    _lws_protocols_ptr = (lws_protocols *)realloc(_lws_protocols_ptr, sizeof(lws_protocols) * (_protocols_count + 1));
    if (_lws_protocols_ptr != nullptr)
    {
        lws_protocols *proto = &_lws_protocols_ptr[_protocols_count - 1];
        memset(proto, 0, sizeof(lws_protocols));
        memset(proto + 1, 0, sizeof(lws_protocols));
        proto->name = name;
        proto->callback = callback;
        proto->per_session_data_size = struct_size;
        proto->tx_packet_size = tx_packet_size;
        proto->id = _protocols_count - 1;
        proto->user = this;
    }
    else
    {
        SIHD_LOG(error, "HttpServer: failed to add protocol: {}", name);
        --_protocols_count;
    }
    return _lws_protocols_ptr != nullptr;
}

bool HttpServer::add_websocket(const char *name, IWebsocketHandler *handler, size_t tx_packet_size)
{
    bool ret = this->add_protocol(name,
                                  HttpServer::_global_websocket_lws_callback,
                                  sizeof(WebsocketSession),
                                  tx_packet_size);
    if (ret)
    {
        _websocket_handler_lst.resize(_protocols_count);
        _websocket_handler_lst[_protocols_count - 1] = handler;
    }
    return ret;
}

HttpServer::LwsPollingScheduler::LwsPollingScheduler(HttpServer *srv): server(srv) {}

HttpServer::LwsPollingScheduler::~LwsPollingScheduler() {}

bool HttpServer::LwsPollingScheduler::run()
{
    this->server->check_all_protocols();
    return true;
}

} // namespace sihd::http

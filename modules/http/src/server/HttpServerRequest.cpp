#include <sihd/http/HttpRequest.hpp>
#include <sihd/http/HttpResponse.hpp>
#include <sihd/http/HttpStatus.hpp>
#include <sihd/http/IHttpAuthenticator.hpp>
#include <sihd/http/IHttpFilter.hpp>
#include <sihd/http/WebService.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/str.hpp>

#include "HttpServerImpl.hpp"

namespace sihd::http
{

SIHD_LOGGER;

using namespace sihd::util;
using namespace sihd::sys;

void HttpServer::Impl::session_init(HttpSession *session)
{
    session->init();
    std::lock_guard sl(sessions_mutex);
    active_sessions.insert(session);
}

void HttpServer::Impl::session_cleanup(HttpSession *session)
{
    std::lock_guard sl(sessions_mutex);
    if (active_sessions.erase(session) == 0)
        return;
    session->clean();
}

int HttpServer::Impl::_global_http_lws_callback(struct lws *wsi,
                                                 enum lws_callback_reasons reason,
                                                 void *user,
                                                 void *in,
                                                 size_t len)
{
    HttpServer *srv = (HttpServer *)lws_context_user(lws_get_context(wsi));
    return srv->_impl->_lws_http_callback(wsi, reason, user, in, len);
}

int HttpServer::Impl::_lws_http_callback(struct lws *wsi,
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
            break;
        }
        case LWS_CALLBACK_PROCESS_HTML:
        {
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
            break;
        case LWS_CALLBACK_HTTP_BIND_PROTOCOL:
            if (session != nullptr)
                this->session_init(session);
            break;
        case LWS_CALLBACK_HTTP_DROP_PROTOCOL:
            if (session != nullptr)
                this->session_cleanup(session);
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
            lws_get_peer_addresses(wsi, fd, client_name, sizeof(client_name), client_ip, sizeof(client_ip));
            SIHD_LOG(debug, "HttpServer: received client connect from {} ({})", client_name, client_ip);
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

HttpRequest::RequestType HttpServer::Impl::get_request_type(struct lws *wsi)
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
    else if (lws_hdr_total_length(wsi, WSI_TOKEN_PATCH_URI))
        return HttpRequest::Patch;
    else if (lws_hdr_total_length(wsi, WSI_TOKEN_HEAD_URI))
        return HttpRequest::Head;
    return HttpRequest::None;
}

int HttpServer::Impl::on_http_request(HttpSession *session, std::string_view path)
{
    SIHD_LOG(debug, "HttpServer: {} request: {}", HttpRequest::type_str(session->request_type), path);

    if (session->request_type == HttpRequest::Options && !default_cors_origin.empty())
    {
        HttpResponse response;
        response.set_status(HttpStatus::NoContent);
        response.http_header().set_server(default_server_name);
        response.http_header().set_header("access-control-allow-origin:", default_cors_origin);
        response.http_header().set_header("access-control-allow-methods:", "GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS");
        response.http_header().set_header("access-control-allow-headers:", "content-type, authorization");
        response.http_header().set_header("access-control-max-age:", "86400");
        response.http_header().set_content_length(0);
        this->send_http_headers(session->wsi, response);
        return 0;
    }

    {
        std::optional<std::string> auth_header = this->get_header(session->wsi, WSI_TOKEN_HTTP_AUTHORIZATION);
        if (auth_header.has_value())
            session->cached_auth_header = std::move(*auth_header);
    }

    int rc = 0;
    std::string resource_path;
    if (server->get_resource_path(path, resource_path))
    {
        std::string type = fmt::format("{}; charset={}", mime.get(fs::extension(resource_path)), encoding);
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
        return this->send_404(session->wsi, "<html><body><h1>404 file not found</h1></body></html>") ? 0 : -1;

    return rc;
}

int HttpServer::Impl::on_http_body(HttpSession *session, const uint8_t *buf, size_t size)
{
    if (session->content != nullptr)
    {
        size_t current_offset = session->content_size;
        session->content->copy_from(buf, size, current_offset);
        session->content_size += size;
    }
    return 0;
}

int HttpServer::Impl::on_http_body_end(HttpSession *session)
{
    int rc = 0;
    if (session->request != nullptr && session->content != nullptr)
    {
        session->request->set_content(*session->content);
        WebService *webservice = this->_get_webservice_from_path(session->request->url());
        if (webservice != nullptr)
            rc = this->serve_webservice(session, webservice, *session->request);
        session->new_request();
    }
    else
        lws_return_http_status(session->wsi, HTTP_STATUS_OK, nullptr);
    return rc;
}

WebService *HttpServer::Impl::_get_webservice_from_path(std::string_view path, std::string *webservice_name)
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

bool HttpServer::Impl::check_webservices(HttpSession *session, std::string_view path)
{
    std::string webservice_name;
    WebService *webservice = this->_get_webservice_from_path(path, &webservice_name);
    if (webservice == nullptr)
        return false;
    if (session->request_type == HttpRequest::Post || session->request_type == HttpRequest::Put
        || session->request_type == HttpRequest::Patch)
    {
        auto content_length_header_opt = this->get_header(session->wsi, WSI_TOKEN_HTTP_CONTENT_LENGTH);
        if (content_length_header_opt.has_value() == false)
            throw std::runtime_error(
                fmt::format("failed to copy content length header serving webservice: {}", webservice_name));

        std::string & content_length_header = content_length_header_opt.value();
        if (content_length_header.empty())
            return false;
        const auto content_length_opt = str::convert_from_string<size_t>(content_length_header, 10);
        if (content_length_opt.has_value() == false)
            throw std::runtime_error("content length header has no number");
        const size_t content_length_header_size = *content_length_opt;

        if (content_length_header_size == 0)
        {
            HttpRequest request = HttpRequest(path, this->get_uri_args(session->wsi), session->request_type);
            if (this->serve_webservice(session, webservice, request) == false)
                return false;
        }
        else
        {
            session->content_size = 0;
            session->content = std::make_unique<ArrUByte>();
            session->request
                = std::make_unique<HttpRequest>(path, this->get_uri_args(session->wsi), session->request_type);
            session->content->resize(content_length_header_size);
            session->should_complete_transaction = false;
        }
    }
    else
    {
        HttpRequest request = HttpRequest(path, this->get_uri_args(session->wsi), session->request_type);
        if (this->serve_webservice(session, webservice, request) == false)
            return false;
    }
    return true;
}

std::unordered_map<std::string, std::string>
    HttpServer::Impl::parse_query_params(const std::vector<std::string> & uri_args)
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

bool HttpServer::Impl::check_and_apply_auth(HttpSession *session, WebService *webservice, HttpRequest & request)
{
    IHttpAuthenticator *authenticator = this->get_authenticator_for(webservice);
    if (authenticator == nullptr)
        return true;

    std::string_view auth_header_value;
    if (session->cached_auth_header.has_value())
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

bool HttpServer::Impl::send_response(HttpSession *session, HttpResponse & response)
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

bool HttpServer::Impl::serve_webservice(HttpSession *session, WebService *webservice, HttpRequest & request)
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

    {
        auto cookie_hdr = this->get_header(session->wsi, WSI_TOKEN_HTTP_COOKIE);
        if (cookie_hdr.has_value() && !cookie_hdr->empty())
        {
            for (auto & part : sihd::util::str::split(*cookie_hdr, ';'))
            {
                auto [name, value]
                    = sihd::util::str::split_pair_view(sihd::util::str::trim(std::string_view(part)), "=");
                if (!name.empty())
                    request.set_cookie(std::string(name), std::string(value));
            }
        }
    }

    if (!check_and_apply_auth(session, webservice, request))
        return true;

    HttpResponse response(&mime);
    response.http_header().set_server(default_server_name);
    if (!webservice->call(rest_of_path, request, response))
        return false;
    send_response(session, response);
    return true;
}

HttpServer::Impl::AuthResult
    HttpServer::Impl::parse_authorization(std::string_view auth_header_value, IHttpAuthenticator *authenticator)
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
        int decoded_len = lws_b64_decode_string(std::string(b64).c_str(), decoded, sizeof(decoded) - 1);
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

IHttpAuthenticator *HttpServer::Impl::get_authenticator_for(WebService *webservice)
{
    if (webservice->authenticator() != nullptr)
        return webservice->authenticator();
    return http_authenticator;
}

std::optional<std::string> HttpServer::Impl::get_header(struct lws *wsi, enum lws_token_indexes idx)
{
    char content_header[SIHD_HTTP_HEADER_VALUE_BUFSIZE + 1];
    ssize_t hdr_len = lws_hdr_copy(wsi, content_header, SIHD_HTTP_HEADER_VALUE_BUFSIZE, idx);
    if (hdr_len < 0)
        return {};
    return std::string(content_header, hdr_len);
}

std::string HttpServer::Impl::get_client_ip(struct lws *wsi)
{
    char ip_buf[INET6_ADDRSTRLEN];
    ip_buf[0] = 0;
    lws_get_peer_simple(wsi, ip_buf, INET6_ADDRSTRLEN);
    return ip_buf;
}

std::vector<std::string> HttpServer::Impl::get_uri_args(struct lws *wsi)
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

bool HttpServer::Impl::send_404(struct lws *wsi, std::string_view html_404)
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

bool HttpServer::Impl::send_http_no_content(struct lws *wsi, int code)
{
    HttpResponse response;
    response.set_status(code);
    response.http_header().set_server(default_server_name);
    response.http_header().set_content_length(0);
    return this->send_http_headers(wsi, response);
}

bool HttpServer::Impl::send_http_redirect(struct lws *wsi, std::string_view redirect_path, int code)
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

bool HttpServer::Impl::send_http_headers(struct lws *wsi, HttpResponse & response)
{
    int rc;
    uint8_t header_buf[SIHD_HTTP_HEADERS_BUFSIZE];
    memset(header_buf, 0, SIHD_HTTP_HEADERS_BUFSIZE);
    u_char *ptr = header_buf + LWS_PRE;
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
    for (const auto & ck : response.set_cookie_headers())
    {
        rc = lws_add_http_header_by_name(wsi,
                                         (u_char *)"set-cookie:",
                                         (u_char *)ck.c_str(),
                                         ck.size(),
                                         &ptr,
                                         end);
        if (rc)
        {
            SIHD_LOG(error, "HttpHeader: cannot set cookie header");
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
    size_t write_size = ptr - (header_buf + LWS_PRE);
    if (lws_write(wsi, header_buf + LWS_PRE, write_size, LWS_WRITE_HTTP_HEADERS) != (int)write_size)
    {
        SIHD_LOG(error, "HttpServer: failed to write HTTP headers");
        return false;
    }
    return true;
}

} // namespace sihd::http

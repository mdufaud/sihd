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

namespace
{

struct RequestHeader
{
        enum lws_token_indexes token;
        const char *name;
};

// lws only exposes the headers it knows by token
class BufferedBodyStream: public IBodyStream
{
    public:
        explicit BufferedBodyStream(ArrChar & content): _content(content) {}

        virtual IBodyStream *clone() const override { return new BufferedBodyStream(*this); }

        bool on_chunk([[maybe_unused]] const HttpRequest & request, ArrCharView chunk) override
        {
            const bool copied = _content.copy_from(chunk.data(), chunk.size(), _offset);
            _offset += chunk.size();
            return copied;
        }

    private:
        ArrChar & _content;
        size_t _offset = 0;
};

std::string method_list_to_header(const std::vector<HttpRequest::RequestType> & methods)
{
    std::string ret;
    for (HttpRequest::RequestType method : methods)
    {
        if (ret.empty() == false)
            ret += ", ";
        ret += HttpRequest::type_str(method);
    }
    return ret;
}

constexpr RequestHeader request_headers[] = {
    {WSI_TOKEN_HOST, "host"},
    {WSI_TOKEN_HTTP_CONTENT_TYPE, "content-type"},
    {WSI_TOKEN_HTTP_CONTENT_LENGTH, "content-length"},
    {WSI_TOKEN_HTTP_USER_AGENT, "user-agent"},
    {WSI_TOKEN_HTTP_ACCEPT, "accept"},
    {WSI_TOKEN_ORIGIN, "origin"},
    {WSI_TOKEN_HTTP_AUTHORIZATION, "authorization"},
    {WSI_TOKEN_HTTP_COOKIE, "cookie"},
    {WSI_TOKEN_HTTP_REFERER, "referer"},
};

} // namespace

void HttpServer::Impl::session_init(HttpSession *session)
{
    // lws hands over raw memory for its sessions: the object starts living here
    new (session) HttpSession();
    std::lock_guard sl(sessions_mutex);
    active_sessions.insert(session);
}

void HttpServer::Impl::session_cleanup(HttpSession *session)
{
    std::lock_guard sl(sessions_mutex);
    if (active_sessions.erase(session) == 0)
        return;
    session->~HttpSession();
}

int HttpServer::Impl::complete_transaction(HttpSession *session, int rc)
{
    if (session->close_connection)
        // lws only discards a body it can measure: for a refused one it must flush the
        // response and drop the connection, otherwise the next request is swallowed
        return lws_raw_transaction_completed(session->wsi);
    if (session->waiting_body || session->response_pending)
        return rc;
    return lws_http_transaction_completed(session->wsi) ? -1 : rc;
}

bool HttpServer::Impl::check_stream_length(HttpSession *session)
{
    if (session->close_after_stream || session->stream_sent == session->stream_length)
        return true;
    // announcing a length the stream did not reach leaves the client waiting for bytes
    // that will never come: cut the connection instead of truncating silently
    SIHD_LOG(error,
             "HttpServer: stream ended after {} bytes, {} announced",
             session->stream_sent,
             session->stream_length);
    return false;
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
    if (reason == LWS_CALLBACK_HTTP_BIND_PROTOCOL && session != nullptr)
        this->session_init(session);
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
            rc = this->complete_transaction(session, rc);
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
            rc = this->complete_transaction(session, this->on_http_body_end(session));
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
                // the provider runs in the lws callback frame: an exception would terminate
                bool has_more = false;
                try
                {
                    has_more = session->stream_provider(chunk);
                }
                catch (const std::exception & error)
                {
                    SIHD_LOG(error, "HttpServer: stream provider threw: {}", error.what());
                    session->stream_provider = nullptr;
                    rc = -1;
                    break;
                }
                if (has_more && chunk.size() == 0)
                {
                    SIHD_LOG(error, "HttpServer: stream provider produced an empty chunk");
                    session->stream_provider = nullptr;
                    rc = -1;
                    break;
                }
                if (session->close_after_stream == false
                    && chunk.size() > session->stream_length - session->stream_sent)
                {
                    SIHD_LOG(error, "HttpServer: stream provider wrote past the announced length");
                    session->stream_provider = nullptr;
                    rc = -1;
                    break;
                }
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
                        SIHD_LOG(error, "HttpServer: could not write the full stream chunk to the client");
                        rc = -1;
                        break;
                    }
                    session->stream_sent += chunk.size();
                }
                if (has_more)
                    lws_callback_on_writable(wsi);
                else
                {
                    session->stream_provider = nullptr;
                    if (this->check_stream_length(session) == false)
                        rc = -1;
                    else if (session->close_after_stream || lws_http_transaction_completed(wsi))
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
        case LWS_CALLBACK_HTTP_BIND_PROTOCOL:
        case LWS_CALLBACK_PROTOCOL_INIT:
        case LWS_CALLBACK_PROTOCOL_DESTROY:
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

                for (const RequestHeader & header : request_headers)
                {
                    auto val = this->get_header(wsi, header.token);
                    if (val.has_value() && !val->empty())
                        info.headers[header.name] = std::move(*val);
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

bool HttpServer::Impl::is_cors_preflight(struct lws *wsi)
{
    // a preflight carries both headers: a plain OPTIONS must reach the routes
    if (default_cors_origin.empty())
        return false;
    const std::optional<std::string> origin = this->get_header(wsi, WSI_TOKEN_ORIGIN);
    if (origin.has_value() == false || origin->empty())
        return false;
    const std::optional<std::string> requested = this->get_custom_header(wsi, "access-control-request-method:");
    return requested.has_value() && requested->empty() == false;
}

int HttpServer::Impl::on_http_request(HttpSession *session, std::string_view path)
{
    SIHD_LOG(debug, "HttpServer: {} request: {}", HttpRequest::type_str(session->request_type), path);

    if (session->request_type == HttpRequest::Options && this->is_cors_preflight(session->wsi))
    {
        HttpResponse response;
        response.set_status(HttpStatus::NoContent);
        response.http_header().set_server(default_server_name);
        response.http_header().set_header("access-control-allow-methods:",
                                          "GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS");
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
    if (const auto resource_path = server->get_resource_path(path))
    {
        std::string type = fmt::format("{}; charset={}", mime.get(fs::extension(resource_path.value())), encoding);
        if (lws_serve_http_file(session->wsi, resource_path.value().c_str(), type.c_str(), nullptr, 0) < 0)
            rc = -1;
        session->response_pending = true;
        return rc;
    }
    try
    {
        if (this->check_webservices(session, path))
            return session->rc;
    }
    catch (const std::exception & error)
    {
        SIHD_LOG(error, "HttpServer: {}", error.what());
        return -1;
    }

    if (this->serve_not_found(session, path))
        return 0;
    return -1;
}

int HttpServer::Impl::on_http_body(HttpSession *session, const uint8_t *buf, size_t size)
{
    if (session->reject_body || session->body_stream == nullptr)
        return 0;

    // lws hands over whatever arrives: the announced length is the only bound
    if (session->content_size + size > session->content_length)
    {
        SIHD_LOG(warning, "HttpServer: body longer than announced");
        this->reject_body(session, HttpStatus::PayloadTooLarge);
        return 0;
    }

    // the stream runs in the lws callback frame: an exception would terminate
    bool streamed = false;
    try
    {
        streamed = session->body_stream->on_chunk(*session->request, {(const char *)buf, size});
    }
    catch (const std::exception & error)
    {
        SIHD_LOG(error, "HttpServer: body stream threw: {}", error.what());
    }
    if (streamed == false)
    {
        this->reject_body(session, HttpStatus::InternalServerError);
        return 0;
    }
    session->content_size += size;
    return 0;
}

int HttpServer::Impl::on_http_body_end(HttpSession *session)
{
    session->waiting_body = false;
    session->body_stream.reset();
    if (session->reject_body)
    {
        session->new_request();
        return 0;
    }
    if (session->request == nullptr)
    {
        // no request owns this body: the connection state is unknown, drop it instead of answering
        SIHD_LOG(error, "HttpServer: body completed without a request");
        session->new_request();
        return -1;
    }
    return this->serve_body(session);
}

bool HttpServer::Impl::serve_bodyless(HttpSession *session, WebService *webservice, HttpRequest & request)
{
    if (this->check_and_apply_auth(session, webservice, request) == false)
        return true;
    return this->serve_webservice(session, webservice, request);
}

void HttpServer::Impl::populate_request(HttpSession *session, HttpRequest & request)
{
    request.set_client_ip(this->get_client_ip(session->wsi));
    request.set_query_params(parse_query_params(request.uri_args()));

    auto cookie_hdr = this->get_header(session->wsi, WSI_TOKEN_HTTP_COOKIE);
    if (cookie_hdr.has_value() && cookie_hdr->empty() == false)
    {
        for (auto & part : str::split(*cookie_hdr, ';'))
        {
            auto [name, value] = str::split_pair_view(str::trim(std::string_view(part)), "=");
            if (name.empty() == false)
                request.set_cookie(std::string(name), std::string(value));
        }
    }
}

int HttpServer::Impl::serve_body(HttpSession *session)
{
    if (session->content.empty() == false)
        session->request->set_content_view({session->content.buf(), session->content_size});

    WebService *webservice = this->_get_webservice_from_path(session->request->url());
    int rc = 0;
    if (webservice == nullptr || this->serve_webservice(session, webservice, *session->request) == false)
        rc = this->serve_not_found(session, session->request->url()) ? 0 : -1;
    // a streaming response outlives this callback: it keeps the session until it ends
    if (session->response_pending == false)
        session->new_request();
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

    const bool has_body = session->request_type == HttpRequest::Post || session->request_type == HttpRequest::Put
                          || session->request_type == HttpRequest::Patch;
    if (has_body == false)
    {
        HttpRequest request(path, this->get_uri_args(session->wsi), session->request_type);
        this->apply_request_headers(session->wsi, request);
        this->populate_request(session, request);
        return this->serve_bodyless(session, webservice, request);
    }

    // the request outlives the headers callback: the body callbacks serve it
    session->request = std::make_unique<HttpRequest>(path, this->get_uri_args(session->wsi), session->request_type);
    HttpRequest & request = *session->request;
    this->apply_request_headers(session->wsi, request);
    this->populate_request(session, request);

    const std::optional<size_t> content_length = this->resolve_body_length(session, request, webservice_name);
    if (content_length.has_value() == false)
        return true;
    if (*content_length == 0)
        return this->serve_bodyless(session, webservice, request);

    session->content_length = *content_length;
    session->content_size = 0;
    if (this->check_and_apply_auth(session, webservice, request) == false)
        return true;
    if (this->prepare_body_stream(session, webservice, request, *content_length, webservice_name) == false)
        return true;

    session->waiting_body = true;
    return true;
}

std::optional<size_t> HttpServer::Impl::resolve_body_length(HttpSession *session,
                                                            HttpRequest & request,
                                                            const std::string & webservice_name)
{
    // lws does not decode chunked bodies and assumes a body for any post-like request:
    // the announced length is the only body the server can bound or discard
    const std::optional<std::string> transfer_encoding = this->get_header(session->wsi,
                                                                          WSI_TOKEN_HTTP_TRANSFER_ENCODING);
    // lws answers an empty token for an absent header: presence comes from the parsed request
    const std::string_view announced_length = request.http_header().find("content-length");
    if ((transfer_encoding.has_value() && transfer_encoding->empty() == false) || announced_length.empty())
    {
        SIHD_LOG(warning, "HttpServer: no length for body of request to '{}'", webservice_name);
        this->reject_body(session, HttpStatus::LengthRequired);
        return {};
    }
    const std::optional<size_t> content_length = request.http_header().content_length();
    if (content_length.has_value() == false)
    {
        SIHD_LOG(warning, "HttpServer: invalid content length for request to '{}'", webservice_name);
        this->reject_body(session, HttpStatus::BadRequest);
        return {};
    }
    if (this->max_request_size > 0 && *content_length > this->max_request_size)
    {
        SIHD_LOG(warning,
                 "HttpServer: request of {} bytes over the {}-byte limit to '{}'",
                 *content_length,
                 this->max_request_size,
                 webservice_name);
        this->reject_body(session, HttpStatus::PayloadTooLarge);
        return {};
    }
    return content_length;
}

bool HttpServer::Impl::prepare_body_stream(HttpSession *session,
                                           WebService *webservice,
                                           HttpRequest & request,
                                           size_t content_length,
                                           const std::string & webservice_name)
{
    // a custom stream consumes the body: multipart has nothing left to parse
    const std::optional<std::string_view> content_type = request.http_header().content_type();
    if (webservice->body_stream() != nullptr && content_type.has_value() && Multipart::is_content_type(*content_type))
    {
        SIHD_LOG(error, "HttpServer: '{}' streams its body and cannot also parse multipart", webservice_name);
        this->reject_body(session, HttpStatus::InternalServerError);
        return false;
    }

    if (IBodyStream *stream = webservice->body_stream(); stream != nullptr)
        session->body_stream.reset(stream->clone());
    else
    {
        // buffering reads the whole body in memory: streaming is the path for bigger payloads
        if (content_length > this->max_buffered_request_size)
        {
            SIHD_LOG(warning,
                     "HttpServer: buffered body of {} bytes over the {}-byte limit to '{}'",
                     content_length,
                     this->max_buffered_request_size,
                     webservice_name);
            this->reject_body(session, HttpStatus::PayloadTooLarge);
            return false;
        }
        if (session->content.resize(content_length) == false)
        {
            SIHD_LOG(error,
                     "HttpServer: cannot allocate {} bytes for the body of request to '{}'",
                     content_length,
                     webservice_name);
            this->reject_body(session, HttpStatus::InternalServerError);
            return false;
        }
        session->body_stream = std::make_unique<BufferedBodyStream>(session->content);
    }
    if (session->body_stream == nullptr)
    {
        SIHD_LOG(error, "HttpServer: cannot stream the body of request to '{}'", webservice_name);
        this->reject_body(session, HttpStatus::InternalServerError);
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
        this->reject_body(session, response);
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
        // without a length the client only knows the body ended when the connection closes
        const std::optional<size_t> content_length = response.http_header().content_length();
        session->close_after_stream = content_length.has_value() == false;
        session->stream_length = content_length.value_or(0);
        if (session->close_after_stream)
            response.http_header().set_header("connection:", "close");
        if (this->send_http_headers(session->wsi, response) == false)
        {
            session->rc = -1;
            return false;
        }
        session->stream_provider = std::move(response.stream_provider());
        session->response_pending = true;
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
    if (this->send_http_headers(session->wsi, response) == false)
    {
        session->rc = -1;
        return false;
    }
    if (data.size() > 0)
    {
        if (lws_write(session->wsi, (unsigned char *)data.buf(), data.size(), LWS_WRITE_HTTP_FINAL) < (int)data.size())
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

    {
        const std::optional<std::string_view> content_type = request.http_header().content_type();
        if (content_type.has_value() && Multipart::is_content_type(*content_type))
        {
            auto multipart = Multipart::parse(request.content().cpp_str_view(), *content_type);
            if (multipart.has_value() == false)
            {
                SIHD_LOG(warning, "HttpServer: malformed multipart body: {}", multipart.error().message);
                this->send_http_error(session, HttpStatus::BadRequest);
                return true;
            }
            request.set_multipart(std::move(*multipart));
        }
    }

    HttpResponse response(&mime);
    response.http_header().set_server(default_server_name);
    // the handler runs in the lws callback frame: an exception would terminate
    bool called = false;
    try
    {
        called = webservice->call(rest_of_path, request, response);
    }
    catch (const std::exception & error)
    {
        SIHD_LOG(error, "HttpServer: handler of '{}' threw: {}", rest_of_path, error.what());
        this->send_http_error(session, HttpStatus::InternalServerError);
        return true;
    }
    if (called == false)
    {
        const std::vector<HttpRequest::RequestType> allowed = webservice->allowed_methods(rest_of_path);
        if (allowed.empty())
            return false;
        SIHD_LOG(debug,
                 "HttpServer: {} not allowed on '{}'",
                 HttpRequest::type_str(request.request_type()),
                 rest_of_path);
        this->send_http_error(session, HttpStatus::MethodNotAllowed, method_list_to_header(allowed));
        return true;
    }
    send_response(session, response);
    return true;
}

HttpServer::Impl::AuthResult HttpServer::Impl::parse_authorization(std::string_view auth_header_value,
                                                                   IHttpAuthenticator *authenticator)
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
        const auto decoded = sihd::util::str::from_base64(auth_header_value.substr(basic_prefix.size()));
        if (!decoded)
            SIHD_LOG(warning, "HttpServer: could not decode the basic authorization header");
        else
        {
            std::string_view credentials((const char *)decoded->data(), decoded->size());
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

std::optional<std::string> HttpServer::Impl::get_custom_header(struct lws *wsi, std::string_view name)
{
    char header_value[SIHD_HTTP_HEADER_VALUE_BUFSIZE + 1];
    const int hdr_len = lws_hdr_custom_copy(wsi, header_value, sizeof(header_value), name.data(), (int)name.size());
    if (hdr_len < 0)
        return {};
    return std::string(header_value, hdr_len);
}

void HttpServer::Impl::apply_request_headers(struct lws *wsi, HttpRequest & request)
{
    for (const RequestHeader & header : request_headers)
    {
        auto value = this->get_header(wsi, header.token);
        if (value.has_value() && value->empty() == false)
            request.http_header().set_header(header.name, std::move(*value));
    }
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
    if (this->send_http_headers(wsi, response) == false)
        return false;
    return lws_write(wsi, (unsigned char *)html_404.data(), html_404.size(), LWS_WRITE_HTTP_FINAL)
           == (int)html_404.size();
}

bool HttpServer::Impl::serve_not_found(HttpSession *session, std::string_view path)
{
    static constexpr std::string_view default_404 = "<html><body><h1>404 file not found</h1></body></html>";
    SIHD_LOG(debug, "HttpServer: no entry point for '{}'", path);

    std::string page;
    if (page_404_path.empty() == false)
    {
        if (auto content = fs::read_all(page_404_path); content.has_value())
            return this->send_404(session->wsi, *content);
        SIHD_LOG(warning, "HttpServer: cannot read the 404 page '{}'", page_404_path);
    }
    return this->send_404(session->wsi, default_404);
}

bool HttpServer::Impl::send_http_error(HttpSession *session, int code, std::string_view allow)
{
    HttpResponse response;
    response.set_status(code);
    response.http_header().set_server(default_server_name);
    if (allow.empty() == false)
        response.http_header().set_header("allow:", allow);
    response.http_header().set_content_length(0);
    if (session->close_connection)
        response.http_header().set_header("connection:", "close");
    return this->send_http_headers(session->wsi, response);
}

bool HttpServer::Impl::reject_body(HttpSession *session, HttpResponse & response)
{
    session->reject_body = true;
    session->close_connection = true;
    response.http_header().set_header("connection:", "close");
    return this->send_http_headers(session->wsi, response);
}

bool HttpServer::Impl::reject_body(HttpSession *session, int code)
{
    session->reject_body = true;
    session->close_connection = true;
    return this->send_http_error(session, code);
}

bool HttpServer::Impl::send_http_redirect(struct lws *wsi, std::string_view redirect_path, int code)
{
    HttpResponse response;
    response.set_status(code);
    response.http_header().set_server(default_server_name);
    response.http_header().set_accept_charset(encoding);
    response.http_header().set_content_type(mime.get("html"), encoding);
    response.http_header().set_content_length(0);
    response.http_header().set_header((const char *)lws_token_to_string(WSI_TOKEN_HTTP_LOCATION), redirect_path);
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

    // lws adds it through the mount path only: every other response gets it here
    if (default_cors_origin.empty() == false)
        headers.set_header("access-control-allow-origin:", default_cors_origin);

    rc = lws_add_http_header_status(wsi, response.status(), &ptr, end);
    if (rc)
    {
        SIHD_LOG(error, "HttpHeader: cannot set status");
        return false;
    }
    for (const auto & [name, value] : headers.headers())
    {
        if (name == "content-length:")
        {
            // lws needs the length to know the body ended and serve the next request
            const auto length = str::convert_from_string<size_t>(value, 10);
            if (length.has_value() == false)
            {
                SIHD_LOG(error, "HttpServer: invalid response content-length '{}'", value);
                return false;
            }
            rc = lws_add_http_header_content_length(wsi, (lws_filepos_t)*length, &ptr, end);
        }
        else
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
        rc = lws_add_http_header_by_name(wsi, (u_char *)"set-cookie:", (u_char *)ck.c_str(), ck.size(), &ptr, end);
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

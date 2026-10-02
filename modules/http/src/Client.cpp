#include "Client.hpp"

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <fmt/core.h>

#include <sihd/curl.hpp>
#include <sihd/http/HttpRequest.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Url.hpp>
#include <sihd/util/tools.hpp>

using enum sihd::util::ErrorCode;
using namespace sihd::util;

namespace sihd::http
{

SIHD_NEW_LOGGER("sihd::http");

namespace
{

std::string url_with_parameters(std::string_view url, const std::map<std::string, std::string> & parameters)
{
    if (parameters.empty())
        return std::string(url);
    Url parsed(url);
    for (const auto & [key, val] : parameters)
        parsed.query_params[key] = val;
    return parsed.encode();
}

sihd::curl::Proxy to_curl_proxy(ProxyType type)
{
    switch (type)
    {
        case ProxyType::Socks4:
            return sihd::curl::Proxy::Socks4;
        case ProxyType::Socks5:
            return sihd::curl::Proxy::Socks5;
        case ProxyType::Http:
            break;
    }
    return sihd::curl::Proxy::Http;
}

} // namespace

struct Client::Impl
{
        sihd::curl::Request request;
        std::string content;
        bool overflow = false;

        // the per-transfer state nothing else restates: the response buffer
        void begin_transfer()
        {
            overflow = false;
            content.clear();
            request.clear_mime();
        }

        Client::Result configure(std::string_view url,
                                 HttpRequest::RequestType type,
                                 const RequestOptions & options,
                                 HttpResponse & response)
        {
            if (sihd::util::tools::maximum_one_true(!options.username.empty() && !options.password.empty(),
                                                    !options.token.empty())
                == false)
            {
                return std::unexpected(Error(invalid_argument, "there can only be one authentication method"));
            }
            if ((options.username.empty() == false) != (options.password.empty() == false))
            {
                return std::unexpected(Error(invalid_argument, "authentication needs both username and password"));
            }

            request.set_verbose(options.verbose);
            request.set_url(url_with_parameters(url, options.parameters));

            request.set_header_callback([&response](sihd::util::ArrByteView data) {
                const std::string_view line = str::rtrim(std::string_view((const char *)data.buf(), data.size()));
                // the status line and final empty line are protocol, not headers
                if (line.empty() || line.starts_with("HTTP/"))
                    return true;
                SIHD_UNEXPECTED_LOG(response.http_header().add_header_from_str(line));
                return true;
            });

            if (options.progress)
            {
                // the closure outlives this call: capture the callback, not a reference to options
                request.set_progress_callback([progress = options.progress](sihd::curl::XferProgress prog) {
                    return progress(Progress {prog.dltotal, prog.dlnow, prog.ultotal, prog.ulnow});
                });
            }
            else
            {
                request.set_progress_callback(nullptr);
            }

            request.set_timeout(options.timeout);
            request.set_connect_timeout(options.connect_timeout);
            request.set_follow_location(options.follow_location);
            request.set_ssl_verify(options.ssl_verify_peer, options.ssl_verify_host);
            request.set_user_agent(options.user_agent);

            // restate the conditional options every time: the handle keeps the
            // previous transfer's proxy, credentials and encoding otherwise
            if (options.username.empty() == false)
            {
                request.set_userpass(options.username, options.password);
                request.set_auth(options.digest ? sihd::curl::Auth::Digest : sihd::curl::Auth::Basic);
            }
            else
            {
                request.set_userpass("", "");
            }

            if (options.proxy.has_value())
            {
                request.set_proxy(options.proxy.value(), to_curl_proxy(options.proxy_type));
                if (options.proxy_username.empty() == false)
                    request.set_proxy_auth(options.proxy_username, options.proxy_password);
                else
                    request.set_proxy_auth("", "");
            }
            else
            {
                request.clear_proxy();
            }

            if (options.accept_encoding)
                request.set_accept_encoding("");
            else
                request.clear_accept_encoding();

            if (options.http2)
                request.set_http_2_tls(true);

            sihd::curl::HeaderList new_headers;
            if (options.token.empty() == false)
            {
                if (!new_headers.append(fmt::format("Authorization: Bearer {}", options.token)))
                    return std::unexpected(Error(out_of_memory, "could not add authentication header"));
            }
            for (const auto & [header_name, header_value] : options.headers)
            {
                if (!new_headers.append(fmt::format("{}: {}", header_name, header_value)))
                    return std::unexpected(Error(out_of_memory, "could not add header '{}'", header_name));
            }
            request.set_headers(new_headers);

            // only HEAD has no response body: an OPTIONS response carries one
            request.set_nobody(type == HttpRequest::Head);

            // the handle keeps the previous transfer's method and upload flag:
            // every send() states its own, and the custom request resets the one
            // a previous transfer left on the dedicated setters
            switch (type)
            {
                case HttpRequest::Get:
                    request.set_http_get(true);
                    break;
                case HttpRequest::Post:
                    request.set_http_post(true);
                    break;
                default:
                    break;
            }
            request.set_custom_request(HttpRequest::type_str(type));

            return {};
        }

        void set_content_sink(const Streams & streams, const RequestOptions & options)
        {
            if (streams.download != nullptr)
            {
                sihd::sys::File *fp = streams.download;
                request.set_write_callback([fp](sihd::util::ArrByteView data) {
                    auto wrote = fp->write(data.buf(), data.size());
                    if (SIHD_UNEXPECTED_LOG(wrote))
                        return false;
                    return *wrote == data.size();
                });
                return;
            }

            std::string *out = &content;
            size_t max_size = options.max_response_size;
            request.set_write_callback([this, out, max_size](sihd::util::ArrByteView data) {
                if (max_size > 0 && out->size() + data.size() > max_size)
                {
                    overflow = true;
                    return false;
                }
                out->append((const char *)data.buf(), data.size());
                return true;
            });
        }

        void set_multipart(const RequestOptions & options)
        {
            if (options.multipart.empty())
                return;

            // must run after every other option: CURLOPT_MIMEPOST is what makes
            // the transfer a POST_MIME, any method option set later cancels it
            sihd::curl::Mime mime;
            for (const Multipart::Part & part : options.multipart.parts())
            {
                sihd::curl::Mime::Part mime_part = mime.add_part().name(part.name);
                if (part.path.empty() == false)
                    mime_part.file(part.path);
                else
                    mime_part.data(part.data);
                if (part.filename.empty() == false)
                    mime_part.filename(part.filename);
                if (part.content_type.empty() == false)
                    mime_part.content_type(part.content_type);
            }
            request.set_mime(mime);
        }

        void set_body(const Streams & streams)
        {
            if (streams.upload == nullptr)
            {
                request.set_upload(false);
                request.set_body(streams.body);
                return;
            }

            request.set_upload(true);
            request.set_infilesize(streams.upload_size);
            sihd::sys::File *fp = streams.upload;
            request.set_read_callback([fp](char *buffer, size_t capacity) {
                const auto read = fp->read(buffer, capacity);
                // a read error must abort the transfer: reporting it as an end of
                // file would upload a truncated body
                if (SIHD_UNEXPECTED_LOG(read))
                    return sihd::curl::Request::READ_ABORT;
                return read.value();
            });
        }
};

Client::Client(): _impl(std::make_unique<Impl>()) {}

Client::~Client() = default;

void Client::reset()
{
    _impl->request.reset();
    _impl->begin_transfer();
}

Client::Result Client::perform(std::string_view url,
                               const Streams & streams,
                               HttpRequest::RequestType type,
                               const RequestOptions & options,
                               HttpResponse & response)
{
    _impl->begin_transfer();

    // body before options: the method options configure() sets must win over the
    // upload flag and post fields the body wiring leaves on the handle
    _impl->set_content_sink(streams, options);
    _impl->set_body(streams);

    auto configured = _impl->configure(url, type, options, response);
    SIHD_UNEXPECTED_RETURN(configured);

    _impl->set_multipart(options);

    auto performed = _impl->request.perform();
    if (!performed && _impl->overflow == false)
        SIHD_UNEXPECTED_RETURN(performed);

    response.set_status((int)_impl->request.response_code());

    std::string content_type = _impl->request.content_type();
    if (content_type.empty() == false)
        response.set_content_type(content_type);

    if (streams.download == nullptr)
    {
        auto content = response.set_content(_impl->content);
        SIHD_UNEXPECTED_RETURN(content);
    }

    // a truncated response is still a response: overflow() tells it apart
    return {};
}

Client::Result Client::send(std::string_view url,
                            HttpRequest::RequestType type,
                            const RequestOptions & options,
                            HttpResponse & response)
{
    return this->perform(url, Streams {}, type, options, response);
}

Client::Result Client::send(std::string_view url,
                            HttpRequest::RequestType type,
                            sihd::util::ArrCharView body,
                            const RequestOptions & options,
                            HttpResponse & response)
{
    return this->perform(url, Streams {.body = body}, type, options, response);
}

Client::Result Client::send_file(std::string_view url,
                                 std::string_view path,
                                 HttpRequest::RequestType type,
                                 const RequestOptions & options,
                                 HttpResponse & response)
{
    sihd::sys::File file;
    auto opened = file.open(std::string(path), "rb");
    SIHD_UNEXPECTED_RETURN(opened);

    const auto size = file.file_size();
    SIHD_UNEXPECTED_RETURN(size);

    sihd::sys::File *fp = &file;
    return this->perform(url, Streams {.upload = fp, .upload_size = *size}, type, options, response);
}

Client::Result Client::receive_file(std::string_view url,
                                    std::string_view path,
                                    const RequestOptions & options,
                                    HttpResponse & response)
{
    sihd::sys::File file;
    auto opened = file.open(std::string(path), "wb");
    SIHD_UNEXPECTED_RETURN(opened);

    sihd::sys::File *fp = &file;
    return this->perform(url, Streams {.download = fp}, HttpRequest::Get, options, response);
}

bool Client::overflow() const
{
    return _impl->overflow;
}

std::string Client::redirect_url() const
{
    return _impl->request.redirect_url();
}

long Client::new_connection_count() const
{
    return _impl->request.new_connection_count();
}

bool Client::enable_cookie_engine()
{
    return _impl->request.set_cookie_engine();
}

bool Client::add_cookie(std::string_view cookie_line)
{
    return _impl->request.add_cookie(cookie_line);
}

bool Client::clear_cookies()
{
    return _impl->request.clear_cookies();
}

std::vector<std::string> Client::cookie_list() const
{
    return _impl->request.cookie_list();
}

bool Client::save_cookies(std::string_view path)
{
    return _impl->request.save_cookies(path);
}

bool Client::load_cookies(std::string_view path)
{
    return _impl->request.load_cookies(path);
}

} // namespace sihd::http

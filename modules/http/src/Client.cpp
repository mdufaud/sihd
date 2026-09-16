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

namespace sihd::http
{

using sihd::util::Url;

SIHD_LOGGER;

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
        sihd::curl::HeaderList headers;
        std::unique_ptr<sihd::curl::Mime> mime;
        std::string content;
        bool overflow = false;

        // the per-transfer state nothing else restates: the response buffer and
        // the previous mime payload. Must run before the body wiring: a mime
        // detach past the post fields would discard them.
        void begin_transfer()
        {
            overflow = false;
            content.clear();
            request.clear_mime();
            mime.reset();
        }

        bool configure(std::string_view url,
                       HttpRequest::RequestType type,
                       const RequestOptions & options,
                       HttpResponse & response)
        {
            if (sihd::util::tools::maximum_one_true(!options.username.empty() && !options.password.empty(),
                                                    !options.token.empty())
                == false)
            {
                SIHD_LOG(error, "Client: there can only be one authentication method");
                return false;
            }
            if ((options.username.empty() == false) != (options.password.empty() == false))
            {
                SIHD_LOG(error, "Client: authentication needs both username and password");
                return false;
            }

            request.set_verbose(options.verbose);
            request.set_url(url_with_parameters(url, options.parameters));

            request.set_header_callback([&response](sihd::util::ArrByteView data) {
                response.http_header().add_header_from_str(std::string_view((const char *)data.buf(), data.size()));
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
                if (new_headers.append(fmt::format("Authorization: Bearer {}", options.token)) == false)
                {
                    SIHD_LOG(error, "Client: could not add authentication header");
                    return false;
                }
            }
            for (const auto & [header_name, header_value] : options.headers)
            {
                if (new_headers.append(fmt::format("{}: {}", header_name, header_value)) == false)
                {
                    SIHD_LOG(error, "Client: could not add header '{}'", header_name);
                    return false;
                }
            }
            // curl only keeps the list pointer: attach the new list before the old
            // one is dropped, and keep it alive in headers until the next attach
            request.set_headers(new_headers);
            headers = std::move(new_headers);

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

            return true;
        }

        void set_content_sink(const Streams & streams, const RequestOptions & options)
        {
            if (streams.download != nullptr)
            {
                sihd::sys::File *fp = streams.download;
                request.set_write_callback([fp](sihd::util::ArrByteView data) {
                    return fp->write(data.buf(), data.size()) == (ssize_t)data.size();
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
            mime = std::make_unique<sihd::curl::Mime>(request.new_mime());
            for (const Multipart::Part & part : options.multipart.parts())
            {
                sihd::curl::Mime::Part mime_part = mime->add_part().name(part.name);
                if (part.path.empty() == false)
                    mime_part.file(part.path);
                else
                    mime_part.data(part.data);
                if (part.filename.empty() == false)
                    mime_part.filename(part.filename);
                if (part.content_type.empty() == false)
                    mime_part.content_type(part.content_type);
            }
            request.set_mime(*mime);
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
                const ssize_t read = fp->read(buffer, capacity);
                // a read error must abort the transfer: reporting it as an end of
                // file would upload a truncated body
                return read < 0 ? sihd::curl::Request::READ_ABORT : (size_t)read;
            });
        }
};

Client::Client(): _impl(std::make_unique<Impl>()) {}

Client::~Client() = default;

void Client::reset()
{
    _impl->request.reset();
    _impl->headers = sihd::curl::HeaderList();
    _impl->begin_transfer();
}

bool Client::perform(std::string_view url,
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

    if (_impl->configure(url, type, options, response) == false)
        return false;

    _impl->set_multipart(options);

    bool ok = _impl->request.perform();

    if (!ok && !_impl->overflow)
    {
        SIHD_LOG(error, "Client: could not perform request: {}", _impl->request.last_error());
        return false;
    }

    response.set_status((int)_impl->request.response_code());

    std::string content_type = _impl->request.content_type();
    if (content_type.empty() == false)
        response.set_content_type(content_type);

    if (streams.download == nullptr)
        response.set_content(_impl->content);

    // a truncated response is still a response: overflow() tells it apart
    return true;
}

bool Client::send(std::string_view url,
                  HttpRequest::RequestType type,
                  const RequestOptions & options,
                  HttpResponse & response)
{
    return this->perform(url, Streams {}, type, options, response);
}

bool Client::send(std::string_view url,
                  HttpRequest::RequestType type,
                  sihd::util::ArrCharView body,
                  const RequestOptions & options,
                  HttpResponse & response)
{
    return this->perform(url, Streams {.body = body}, type, options, response);
}

bool Client::send_file(std::string_view url,
                       std::string_view path,
                       HttpRequest::RequestType type,
                       const RequestOptions & options,
                       HttpResponse & response)
{
    sihd::sys::File file;
    if (file.open(std::string(path), "rb") == false)
    {
        SIHD_LOG(error, "Client: cannot open file: {}", path);
        return false;
    }

    const int64_t size = file.file_size();
    if (size < 0)
    {
        SIHD_LOG(error, "Client: cannot read file size: {}", path);
        return false;
    }

    sihd::sys::File *fp = &file;
    return this->perform(url, Streams {.upload = fp, .upload_size = size}, type, options, response);
}

bool Client::receive_file(std::string_view url,
                          std::string_view path,
                          const RequestOptions & options,
                          HttpResponse & response)
{
    sihd::sys::File file;
    if (file.open(std::string(path), "wb") == false)
    {
        SIHD_LOG(error, "Client: cannot open file for download: {}", path);
        return false;
    }

    sihd::sys::File *fp = &file;
    return this->perform(url, Streams {.download = fp}, HttpRequest::Get, options, response);
}

bool Client::overflow() const
{
    return _impl->overflow;
}

std::string Client::last_error() const
{
    return _impl->request.last_error();
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

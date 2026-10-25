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
#include <sihd/sys/fs.hpp>
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

// every setter has already run: the first failure is the one reported
Result<void> all_set(std::initializer_list<Result<void>> results)
{
    for (const Result<void> & result : results)
    {
        if (!result.has_value())
            return result;
    }
    return {};
}

} // namespace

struct Client::Impl
{
        sihd::curl::Request request;
        std::string content;
        bool overflow = false;
        size_t downloaded = 0;

        // the per-transfer state nothing else restates: the response buffer
        void begin_transfer()
        {
            overflow = false;
            downloaded = 0;
            content.clear();
            // a defunct handle fails this too: perform() reports it
            (void)request.clear_mime();
        }

        Client::Result configure(std::string_view url,
                                 HttpRequest::RequestType type,
                                 const RequestOptions & options,
                                 HttpResponse & response)
        {
            if (tools::maximum_one_true(!options.username.empty() && !options.password.empty(), !options.token.empty())
                == false)
            {
                return std::unexpected(Error(invalid_argument, "there can only be one authentication method"));
            }
            if ((options.username.empty() == false) != (options.password.empty() == false))
            {
                return std::unexpected(Error(invalid_argument, "authentication needs both username and password"));
            }

            auto transfer = this->configure_transfer(url, options, response);
            SIHD_UNEXPECTED_RETURN(transfer);

            // restate the conditional options every time: the handle keeps the
            // previous transfer's proxy, credentials and encoding otherwise
            auto credentials = this->configure_credentials(options);
            SIHD_UNEXPECTED_RETURN(credentials);
            auto proxy = this->configure_proxy(options);
            SIHD_UNEXPECTED_RETURN(proxy);
            auto encoding = options.accept_encoding ? request.set_accept_encoding("") : request.clear_accept_encoding();
            SIHD_UNEXPECTED_RETURN(encoding);
            if (options.http2)
            {
                auto http2 = request.set_http_2_tls(true);
                SIHD_UNEXPECTED_RETURN(http2);
            }

            sihd::curl::HeaderList new_headers;
            if (options.token.empty() == false)
            {
                auto appended = new_headers.append(fmt::format("Authorization: Bearer {}", options.token));
                SIHD_UNEXPECTED_RETURN(appended);
            }
            for (const auto & [header_name, header_value] : options.headers)
            {
                auto appended = new_headers.append(fmt::format("{}: {}", header_name, header_value));
                SIHD_UNEXPECTED_RETURN(appended);
            }
            // empty Expect stops curl waiting 1s for a 100 Continue the server never sends
            auto expect_appended = new_headers.append("Expect:");
            SIHD_UNEXPECTED_RETURN(expect_appended);
            auto headers_set = request.set_headers(new_headers);
            SIHD_UNEXPECTED_RETURN(headers_set);

            return this->configure_method(type);
        }

        Client::Result configure_transfer(std::string_view url, const RequestOptions & options, HttpResponse & response)
        {
            auto resolved = options.resolve.has_value() ? request.set_resolve(options.resolve->host,
                                                                              options.resolve->port,
                                                                              options.resolve->addresses)
                                                        : request.set_resolve({}, 0, {});
            SIHD_UNEXPECTED_RETURN(resolved);

            sihd::curl::Request::ProgressFn on_progress;
            if (options.progress)
            {
                // the closure outlives this call: capture the callback, not a reference to options
                on_progress = [progress = options.progress](sihd::curl::XferProgress prog) {
                    return progress(Progress {prog.dltotal, prog.dlnow, prog.ultotal, prog.ulnow});
                };
            }

            return all_set({request.set_verbose(options.verbose),
                            request.set_url(url_with_parameters(url, options.parameters)),
                            request.set_header_callback([&response](ArrByteView data) {
                                const std::string_view line = str::rtrim(
                                    std::string_view((const char *)data.buf(), data.size()));
                                // the status line and final empty line are protocol, not headers
                                if (line.empty() || line.starts_with("HTTP/"))
                                    return true;
                                SIHD_UNEXPECTED_LOG(response.http_header().add_header_from_str(line));
                                return true;
                            }),
                            request.set_progress_callback(std::move(on_progress)),
                            request.set_timeout(options.timeout),
                            request.set_connect_timeout(options.connect_timeout),
                            request.set_follow_location(options.follow_location),
                            request.set_ssl_verify(options.ssl_verify_peer, options.ssl_verify_host),
                            request.set_user_agent(options.user_agent)});
        }

        Client::Result configure_credentials(const RequestOptions & options)
        {
            if (options.username.empty())
                return request.set_userpass("", "");
            return all_set({request.set_userpass(options.username, options.password),
                            request.set_auth(options.digest ? sihd::curl::Auth::Digest : sihd::curl::Auth::Basic)});
        }

        Client::Result configure_proxy(const RequestOptions & options)
        {
            if (!options.proxy.has_value())
                return request.clear_proxy();
            return all_set({request.set_proxy(options.proxy.value(), to_curl_proxy(options.proxy_type)),
                            request.set_proxy_auth(options.proxy_username,
                                                   options.proxy_username.empty() ? "" : options.proxy_password)});
        }

        Client::Result configure_method(HttpRequest::RequestType type)
        {
            // only HEAD has no response body: an OPTIONS response carries one
            auto nobody = request.set_nobody(type == HttpRequest::Head);
            SIHD_UNEXPECTED_RETURN(nobody);

            // the handle keeps the previous transfer's method and upload flag: every send() states its own,
            // and the custom request resets the one a previous transfer left on the dedicated setters
            Client::Result method;
            if (type == HttpRequest::Get)
                method = request.set_http_get(true);
            else if (type == HttpRequest::Post)
                method = request.set_http_post(true);
            SIHD_UNEXPECTED_RETURN(method);
            return request.set_custom_request(HttpRequest::type_str(type));
        }

        bool within_limit(size_t chunk_size, size_t max_size)
        {
            if (max_size > 0 && downloaded + chunk_size > max_size)
            {
                overflow = true;
                return false;
            }
            downloaded += chunk_size;
            return true;
        }

        Client::Result set_content_sink(const Streams & streams, const RequestOptions & options)
        {
            if (streams.download != nullptr)
            {
                sihd::sys::File *fp = streams.download;
                size_t max_download = options.max_response_size;
                return request.set_write_callback([this, fp, max_download](ArrByteView data) {
                    if (!within_limit(data.size(), max_download))
                        return false;
                    auto wrote = fp->write(data.buf(), data.size());
                    if (SIHD_UNEXPECTED_LOG(wrote))
                        return false;
                    return *wrote == data.size();
                });
            }

            std::string *out = &content;
            size_t max_size = options.max_response_size;
            return request.set_write_callback([this, out, max_size](ArrByteView data) {
                if (!within_limit(data.size(), max_size))
                    return false;
                out->append((const char *)data.buf(), data.size());
                return true;
            });
        }

        Client::Result set_multipart(const RequestOptions & options)
        {
            if (options.multipart.empty())
                return {};

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
            return request.set_mime(mime);
        }

        Client::Result set_body(const Streams & streams)
        {
            if (streams.upload == nullptr)
                return all_set({request.set_upload(false), request.set_body(streams.body)});

            sihd::sys::File *fp = streams.upload;
            return all_set({request.set_upload(true),
                            request.set_infilesize(streams.upload_size),
                            request.set_read_callback([fp](char *buffer, size_t capacity) {
                                const auto read = fp->read(buffer, capacity);
                                // a read error must abort the transfer: reporting it as an end of
                                // file would upload a truncated body
                                if (SIHD_UNEXPECTED_LOG(read))
                                    return sihd::curl::Request::READ_ABORT;
                                return read.value();
                            })});
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
    auto sink = _impl->set_content_sink(streams, options);
    SIHD_UNEXPECTED_RETURN(sink);
    auto body = _impl->set_body(streams);
    SIHD_UNEXPECTED_RETURN(body);

    auto configured = _impl->configure(url, type, options, response);
    SIHD_UNEXPECTED_RETURN(configured);

    auto multipart = _impl->set_multipart(options);
    SIHD_UNEXPECTED_RETURN(multipart);

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
                            ArrCharView body,
                            const RequestOptions & options,
                            HttpResponse & response)
{
    return this->perform(url, Streams {.body = body}, type, options, response);
}

Client::Result Client::send_file(std::string_view url,
                                 const std::string & path,
                                 HttpRequest::RequestType type,
                                 const RequestOptions & options,
                                 HttpResponse & response)
{
    sihd::sys::File file;
    auto opened = file.open(path, "rb");
    SIHD_UNEXPECTED_RETURN(opened);

    const auto size = file.file_size();
    SIHD_UNEXPECTED_RETURN(size);

    sihd::sys::File *fp = &file;
    return this->perform(url, Streams {.upload = fp, .upload_size = *size}, type, options, response);
}

Client::Result Client::receive_file(std::string_view url,
                                    ZStringView path,
                                    const RequestOptions & options,
                                    HttpResponse & response)
{
    const std::string temp_path = fmt::format("{}.{}.part", path, str::generate_random_printable(16));
    sihd::sys::File file;
    auto opened = file.open(temp_path, "wb");
    SIHD_UNEXPECTED_RETURN(opened);

    sihd::sys::File *fp = &file;
    Result performed = this->perform(url, Streams {.download = fp}, HttpRequest::Get, options, response);
    auto closed = file.close();

    const bool complete = performed.has_value() && closed.has_value() && _impl->overflow == false
                          && response.status() >= 200 && response.status() < 300;
    if (complete)
    {
        auto renamed = sihd::sys::fs::rename(temp_path, path);
        if (renamed.has_value())
            return performed;
        (void)sihd::sys::fs::remove_file(temp_path);
        SIHD_UNEXPECTED_RETURN(renamed);
    }
    (void)sihd::sys::fs::remove_file(temp_path);
    if (performed.has_value())
        SIHD_UNEXPECTED_RETURN(closed);
    return performed;
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

Client::Result Client::enable_cookie_engine()
{
    return _impl->request.set_cookie_engine();
}

Client::Result Client::add_cookie(const std::string & cookie_line)
{
    return _impl->request.add_cookie(cookie_line);
}

Client::Result Client::clear_cookies()
{
    return _impl->request.clear_cookies();
}

std::vector<std::string> Client::cookie_list() const
{
    return _impl->request.cookie_list();
}

Client::Result Client::save_cookies(const std::string & path)
{
    return _impl->request.save_cookies(path);
}

Client::Result Client::load_cookies(const std::string & path)
{
    return _impl->request.load_cookies(path);
}

} // namespace sihd::http

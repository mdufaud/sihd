#ifndef __SIHD_HTTP_SRC_CLIENT_HPP__
#define __SIHD_HTTP_SRC_CLIENT_HPP__

#include <cstdint>
#include <cstdio>
#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/http/HttpRequest.hpp>
#include <sihd/http/HttpResponse.hpp>
#include <sihd/http/RequestOptions.hpp>
#include <sihd/sys/File.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Error.hpp>

namespace sihd::http
{

// One transfer at a time on a single curl handle: its connection pool survives reset(), which is
// what reuses connections; perform() re-states each send() and clears the previous transfer
class Client
{
    public:
        using Result = std::expected<void, sihd::util::Error>;

        Client();
        ~Client();

        Client(const Client &) = delete;
        Client & operator=(const Client &) = delete;

        void reset();

        Result send(std::string_view url,
                    HttpRequest::RequestType type,
                    const RequestOptions & options,
                    HttpResponse & response);
        Result send(std::string_view url,
                    HttpRequest::RequestType type,
                    sihd::util::ArrCharView body,
                    const RequestOptions & options,
                    HttpResponse & response);
        Result send_file(std::string_view url,
                         std::string_view path,
                         HttpRequest::RequestType type,
                         const RequestOptions & options,
                         HttpResponse & response);
        Result receive_file(std::string_view url,
                            std::string_view path,
                            const RequestOptions & options,
                            HttpResponse & response);

        // true when the response was cut short by RequestOptions::max_response_size
        bool overflow() const;
        std::string redirect_url() const;
        long new_connection_count() const;

        bool enable_cookie_engine();
        bool add_cookie(std::string_view cookie_line);
        bool clear_cookies();
        std::vector<std::string> cookie_list() const;
        bool save_cookies(std::string_view path);
        bool load_cookies(std::string_view path);

    private:
        struct Streams
        {
                sihd::util::ArrCharView body = {};
                sihd::sys::File *upload = nullptr;
                int64_t upload_size = 0;
                sihd::sys::File *download = nullptr;
        };

        Result perform(std::string_view url,
                       const Streams & streams,
                       HttpRequest::RequestType type,
                       const RequestOptions & options,
                       HttpResponse & response);

        struct Impl;
        std::unique_ptr<Impl> _impl;
};

} // namespace sihd::http

#endif

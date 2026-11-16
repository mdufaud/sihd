#ifndef __SIHD_HTTP_IWEBSOCKETCONNECTION_HPP__
#define __SIHD_HTTP_IWEBSOCKETCONNECTION_HPP__

#include <cstddef>
#include <string_view>

#include <sihd/http/HttpRequest.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::http
{

// valid as long as the AWebsocketHandler it was given to
class IWebsocketConnection
{
    public:
        virtual ~IWebsocketConnection() = default;

        virtual sihd::util::Result<void> send(std::string_view text) = 0;
        virtual sihd::util::Result<void> send_binary(sihd::util::ArrByteView data) = 0;
        virtual void close() = 0;
        virtual bool is_connected() const = 0;
        virtual size_t dropped() const = 0;
        virtual std::string_view protocol_name() const = 0;
        virtual const HttpRequest & request() const = 0;
};

} // namespace sihd::http

#endif

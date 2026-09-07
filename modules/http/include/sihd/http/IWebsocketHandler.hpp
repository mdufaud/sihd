#ifndef __SIHD_HTTP_IWEBSOCKETHANDLER_HPP__
#define __SIHD_HTTP_IWEBSOCKETHANDLER_HPP__

#include <string>
#include <string_view>

#include <sihd/http/WriteProtocol.hpp>
#include <sihd/util/Array.hpp>

namespace sihd::http
{

class IWebsocketHandler
{
    public:
        virtual ~IWebsocketHandler() = default;
        virtual void on_open(std::string_view protocol_name) = 0;
        virtual bool on_read(const sihd::util::ArrChar & array) = 0;
        virtual bool on_write(sihd::util::ArrChar & array, WriteProtocol & protocol) = 0;
        virtual void on_close() = 0;
        virtual void on_peer_close(uint16_t code, std::string_view reason) = 0;
};

} // namespace sihd::http

#endif
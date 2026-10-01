#ifndef __SIHD_NET_INETRECEIVER_HPP__
#define __SIHD_NET_INETRECEIVER_HPP__

#include <expected>

#include <sihd/net/IpAddr.hpp>
#include <sihd/util/Error.hpp>
#include <sihd/util/IArray.hpp>

namespace sihd::net
{

class INetReceiver
{
    public:
        virtual ~INetReceiver() = default;

        virtual std::expected<size_t, sihd::util::Error> receive(IpAddr & addr, sihd::util::IArray & arr) = 0;
        virtual std::expected<size_t, sihd::util::Error> receive(sihd::util::IArray & arr) = 0;
        virtual std::expected<void, sihd::util::Error> close() = 0;
};

} // namespace sihd::net

#endif
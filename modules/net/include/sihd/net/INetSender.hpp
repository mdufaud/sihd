#ifndef __SIHD_NET_INETSENDER_HPP__
#define __SIHD_NET_INETSENDER_HPP__

#include <expected>

#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Error.hpp>

namespace sihd::net
{

class INetSender
{
    public:
        virtual ~INetSender() = default;

        virtual std::expected<size_t, sihd::util::Error> send(sihd::util::ArrCharView view) = 0;
        virtual std::expected<void, sihd::util::Error> send_all(sihd::util::ArrCharView view) = 0;
        virtual std::expected<void, sihd::util::Error> close() = 0;
};

} // namespace sihd::net

#endif
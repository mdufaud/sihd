#ifndef __SIHD_NET_INETSENDER_HPP__
#define __SIHD_NET_INETSENDER_HPP__

#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::net
{

class INetSender
{
    public:
        virtual ~INetSender() = default;

        virtual sihd::util::Result<size_t> send(sihd::util::ArrCharView view) = 0;
        virtual sihd::util::Result<void> send_all(sihd::util::ArrCharView view) = 0;
        virtual sihd::util::Result<void> close() = 0;
};

} // namespace sihd::net

#endif
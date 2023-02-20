#ifndef __SIHD_NET_INETSENDER_HPP__
#define __SIHD_NET_INETSENDER_HPP__

#include <sihd/util/ArrayView.hpp>

namespace sihd::net
{

class INetSender
{
    public:
        virtual ~INetSender() {};

        virtual ssize_t send(sihd::util::ArrCharView view) = 0;
        virtual bool send_all(sihd::util::ArrCharView view) = 0;
        virtual bool close() = 0;
};

} // namespace sihd::net

#endif
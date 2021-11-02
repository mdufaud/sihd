#ifndef __SIHD_NET_INETSENDER_HPP__
# define __SIHD_NET_INETSENDER_HPP__

# include <sihd/util/IArray.hpp>

namespace sihd::net
{

class INetSender
{
    public:
        virtual ~INetSender() {};

        virtual ssize_t send(const sihd::util::IArray & arr) = 0;
        virtual bool send_all(const sihd::util::IArray & arr) = 0;
        virtual bool close() = 0;
};

}

#endif 
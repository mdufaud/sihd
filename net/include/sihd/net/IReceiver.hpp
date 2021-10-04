#ifndef __SIHD_NET_IRECEIVER_HPP__
# define __SIHD_NET_IRECEIVER_HPP__

# include <cstddef>

namespace sihd::net
{

class IReceiver
{
    public:
        virtual ~IReceiver() {};

        virtual ssize_t receive(void *buf, size_t size) = 0;

};

}

#endif 
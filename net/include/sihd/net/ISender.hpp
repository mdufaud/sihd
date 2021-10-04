#ifndef __SIHD_NET_ISENDER_HPP__
# define __SIHD_NET_ISENDER_HPP__

# include <sys/types.h>

namespace sihd::net
{

class ISender
{
    public:
        virtual ~ISender() {};

        virtual ssize_t send(void *buf, size_t size) = 0;
};

}

#endif 
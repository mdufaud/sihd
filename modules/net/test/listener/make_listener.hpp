#ifndef __SIHD_NET_MAKE_LISTENER_HPP__
#define __SIHD_NET_MAKE_LISTENER_HPP__

#include <gtest/gtest.h>

#include <sihd/net/IpAddr.hpp>
#include <sihd/net/Socket.hpp>

namespace test
{

inline void make_listener(sihd::net::Socket & server, const sihd::net::IpAddr & addr)
{
    ASSERT_TRUE(server.open(AF_INET, SOCK_STREAM, 0));
    ASSERT_TRUE(server.set_reuseaddr(true));
    ASSERT_TRUE(server.bind(addr));
    ASSERT_TRUE(server.listen(2));
}

} // namespace test

#endif

#ifndef __SIHD_NET_TUN_HPP__
#define __SIHD_NET_TUN_HPP__

#include <memory>
#include <string>

#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::net
{

// one read or write is one IP packet, no link header: /dev/net/tun on linux, wintun.dll on windows
class Tun
{
    public:
        // the linux interface name limit, applied on every platform
        static constexpr size_t max_name_size = 15;

        Tun();
        Tun(Tun && other);
        Tun & operator=(Tun && other);
        ~Tun();

        Tun(const Tun &) = delete;
        Tun & operator=(const Tun &) = delete;

        // linux: a non-blocking descriptor, as android VpnService.Builder.establish() hands one
        static sihd::util::Result<Tun> adopt(int fd);

        // creates the interface without address: the caller configures it - windows loads wintun.dll, admin only
        sihd::util::Result<void> open(const std::string & name);
        void close();

        sihd::util::Result<size_t> read(void *data, size_t size);
        sihd::util::Result<size_t> write(sihd::util::ArrUByteView packet);
        // false when the timeout elapsed without a packet to read
        sihd::util::Result<bool> wait_readable(sihd::util::Duration timeout) const;

        bool is_open() const;
        sihd::util::Result<int> fd() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
};

} // namespace sihd::net

#endif

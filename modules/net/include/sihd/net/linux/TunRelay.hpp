#ifndef __SIHD_NET_LINUX_TUNRELAY_HPP__
#define __SIHD_NET_LINUX_TUNRELAY_HPP__

#include <memory>
#include <string>

#include <sihd/net/Tun.hpp>
#include <sihd/sys/EventPoll.hpp>
#include <sihd/util/ABlockingService.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/IHandler.hpp>
#include <sihd/util/Named.hpp>
#include <sihd/util/Observable.hpp>
#include <sihd/util/build.hpp>

#if !defined(__SIHD_LINUX__)
# error "sihd/net/linux headers are linux only"
#endif

namespace sihd::net::linux_
{

class TunRelay: public sihd::util::Named,
                public sihd::util::ABlockingService,
                public sihd::util::Observable<TunRelay>,
                public sihd::util::IHandler<sihd::sys::EventPoll *>
{
    public:
        enum class Direction
        {
            from_tun,
            to_tun,
        };

        TunRelay(const std::string & name, sihd::util::Node *parent = nullptr);
        ~TunRelay();

        // the outgoing sockets must not route back into this tun
        bool set_tun(Tun && tun);
        bool set_mtu(size_t mtu);
        bool set_udp_timeout(sihd::util::Duration timeout);
        bool set_tcp_timeout(sihd::util::Duration timeout);
        bool set_retransmit_timeout(sihd::util::Duration timeout);

        // the packet being notified: TCP headers written to the tun are the relay's, not the remote peer's
        sihd::util::ArrUByteView packet() const;
        Direction direction() const;

        bool is_ready() const override;
        bool wait_ready(sihd::util::Duration timeout) const override;

    protected:
        void handle(sihd::sys::EventPoll *poll) override;
        bool on_start() override;
        bool on_stop() override;

    private:
        struct Impl;

        std::unique_ptr<Impl> _impl;
};

} // namespace sihd::net::linux_

#endif

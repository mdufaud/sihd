#ifndef __SIHD_NET_PINGER_HPP__
#define __SIHD_NET_PINGER_HPP__

#include <atomic>
#include <memory>

#include <sihd/util/ABlockingService.hpp>
#include <sihd/util/Configurable.hpp>
#include <sihd/util/IHandler.hpp>
#include <sihd/util/Named.hpp>
#include <sihd/util/Poll.hpp>
#include <sihd/util/Stat.hpp>
#include <sihd/util/Waitable.hpp>

#include <sihd/net/IcmpSender.hpp>

namespace sihd::net
{

struct PingEvent
{
        bool sent = false;
        bool received = false;
        bool timeout = false;

        sihd::util::Timestamp trip_time = 0;
        IcmpResponse icmp_response = {};
};

struct PingResult
{
        time_t time_start;
        time_t last_time_sent;
        time_t last_time_received;
        time_t time_end;

        int transmitted;
        int received;
        sihd::util::Stat<int64_t> rtt;

        void clear();
        float packet_loss() const;
        std::string str() const;
};

class Pinger: public sihd::util::Named,
              public sihd::util::Configurable,
              public sihd::util::ABlockingService,
              public sihd::util::Observable<Pinger>,
              public sihd::util::IHandler<IcmpSender *>
{
    public:
        Pinger(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~Pinger();

        bool set_ttl(int ttl);
        bool set_timeout(time_t milliseconds_timeout);
        bool set_interval(time_t milliseconds_interval);
        bool set_client(const IpAddr & client);
        bool set_ping_count(size_t n);

        bool open(bool ipv6 = false);
        bool open_unix();

        // filled when sending, receiving or timeout - call in observable
        const PingEvent & event() const { return _event; }

        // Total stat
        const PingResult & result() const { return _result; }

    protected:
        void handle(IcmpSender *sender) override;

        bool on_start() override;
        bool on_stop() override;

    private:
        void _notify_send();
        void _notify_timeout();
        void _clear_event();

        IpAddr _client;
        size_t _ping_count;

        IcmpSender _sender;
        sihd::util::Waitable _waitable;
        sihd::util::IClock *_clock_ptr;

        std::unique_ptr<sihd::util::ArrByte> _data_ptr;
        int _ttl;
        time_t _ping_ms_interval;

        std::atomic<bool> _stop;
        bool _received_icmp_response;
        int _current_seq;
        PingResult _result;
        PingEvent _event;
};

} // namespace sihd::net

#endif

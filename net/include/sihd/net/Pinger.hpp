#ifndef __SIHD_NET_PINGER_HPP__
#define __SIHD_NET_PINGER_HPP__

#include <atomic>
#include <memory>

#include <sihd/util/Configurable.hpp>
#include <sihd/util/IHandler.hpp>
#include <sihd/util/Named.hpp>
#include <sihd/util/Poll.hpp>
#include <sihd/util/Stat.hpp>
#include <sihd/util/Waitable.hpp>

#include <sihd/net/IcmpSender.hpp>

namespace sihd::net
{

class Pinger;

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
              public sihd::util::Observable<Pinger>,
              public sihd::util::IHandler<IcmpSender *>
{
    public:
        Pinger(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~Pinger();

        bool set_ttl(int ttl);
        bool set_timeout(time_t milliseconds_timeout);
        bool set_interval(time_t milliseconds_interval);

        bool open(bool ipv6 = false);
        bool open_unix();

        // 0 is infinite pings
        bool ping(const IpAddr & client, size_t number = 0);
        bool is_running() const { return _running; }
        void stop();

        const PingEvent & event() const { return _event; }
        const PingResult & result() const { return _result; }

    protected:
        void handle(IcmpSender *sender);

    private:
        void _notify_send();
        void _notify_timeout();
        void _clear_event();

        IcmpSender _sender;
        sihd::util::Waitable _waitable;
        sihd::util::IClock *_clock_ptr;

        std::unique_ptr<sihd::util::ArrByte> _data_ptr;
        int _ttl;
        time_t _ping_ms_interval;

        std::atomic<bool> _running;
        std::atomic<bool> _stop;
        bool _received_icmp_response;
        int _current_seq;
        PingResult _result;
        PingEvent _event;
};

} // namespace sihd::net

#endif

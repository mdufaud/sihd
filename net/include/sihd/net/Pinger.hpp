#ifndef __SIHD_NET_PINGER_HPP__
#define __SIHD_NET_PINGER_HPP__

#include <atomic>
#include <memory>

#include <sihd/net/IcmpSender.hpp>
#include <sihd/sys/Poll.hpp>
#include <sihd/util/ABlockingService.hpp>
#include <sihd/util/Configurable.hpp>
#include <sihd/util/IHandler.hpp>
#include <sihd/util/Named.hpp>
#include <sihd/util/Stat.hpp>
#include <sihd/util/Waitable.hpp>
#include <sihd/util/time.hpp>

namespace sihd::net
{

struct PingEvent
{
        bool sent = false;
        bool received = false;
        bool timeout = false;

        sihd::util::Duration trip_time = 0;
        IcmpResponse icmp_response = {};
};

struct PingResult
{
        sihd::util::Timestamp time_start;
        sihd::util::Timestamp last_time_sent;
        sihd::util::Timestamp last_time_received;
        sihd::util::Timestamp time_end;

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
        bool set_timeout(sihd::util::time::UnixTime milliseconds_timeout);
        bool set_interval(sihd::util::time::UnixTime milliseconds_interval);
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
        sihd::util::time::UnixTime _ping_ms_interval;

        std::atomic<bool> _stop;
        bool _received_icmp_response;
        int _current_seq;
        PingResult _result;
        PingEvent _event;

        uint16_t _expected_id;
};

} // namespace sihd::net

#endif

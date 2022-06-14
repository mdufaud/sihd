#ifndef __SIHD_NET_PINGER_HPP__
# define __SIHD_NET_PINGER_HPP__

# include <sihd/net/IcmpSender.hpp>

# include <sihd/util/Named.hpp>
# include <sihd/util/Configurable.hpp>
# include <sihd/util/IStoppableRunnable.hpp>
# include <sihd/util/Handler.hpp>
# include <sihd/util/Waitable.hpp>
# include <sihd/util/Poll.hpp>

# include <atomic>

namespace sihd::net
{

struct PingResult
{
    time_t time_start;
    time_t last_time_sent;
    time_t time_end;
    int transmitted;
    int received;
    int rtt_min;
    int rtt_avg;
    int rtt_max;
    int rtt_mdev;

    float packet_loss() const;
    std::string str() const;
};

class Pinger:   public sihd::util::Named,
                public sihd::util::Configurable,
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

        const PingResult & result() const { return _result; }

    protected:
        void handle(IcmpSender *sender);

    private:
        struct InternalPingRes
        {
            long long sum;
            long long sum2;
        };

        void _fill_giberish();

        IcmpSender _sender;
        sihd::util::Waitable _waitable;
        sihd::util::IClock *_clock_ptr;

        sihd::util::ArrByte _data;
        int _ttl;
        time_t _ping_ms_interval;

        std::atomic<bool> _running;
        std::atomic<bool> _stop;
        bool _received;
        int _current_seq;
        PingResult _result;
        InternalPingRes _sums;
};

}

#endif
#ifndef __SIHD_NET_ICMPSENDER_HPP__
# define __SIHD_NET_ICMPSENDER_HPP__

# include <sihd/util/Named.hpp>
# include <sihd/util/Configurable.hpp>
# include <sihd/util/IStoppableRunnable.hpp>
# include <sihd/util/Poll.hpp>

# include <sihd/net/Socket.hpp>
# include <sihd/net/Ip.hpp>

namespace sihd::net
{

struct IcmpResponse
{
    IpAddr client;
    bool reached;
    int type;
    int code;
    int ttl;
    int id;
    int seq;
    time_t timestamp;
};

class IcmpSender:   public sihd::util::Named,
                    public sihd::util::Configurable,
                    public sihd::util::IStoppableRunnable,
                    public sihd::util::Observable<IcmpResponse>,
                    public sihd::util::IHandler<sihd::util::Poll *>
{
    public:
        IcmpSender(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~IcmpSender();

        bool open_socket(bool ipv6 = false);
        bool open_socket_unix();
        bool close();

        void set_echo();
        bool set_ttl(int ttl);
        void set_type(int type);
        void set_code(int code);
        void set_id(pid_t id);
        void set_seq(int seq);
        bool set_data(sihd::util::ArrViewByte view);

        ssize_t send_to(const IpAddr & addr);

        bool set_poll_timeout(int milliseconds);
        // poll for x milliseconds - returns true if socket is read
        bool poll(int milliseconds);
        // poll once with configured timeout
        bool poll();

        // calls infinite polling
        bool run();
        // stop polling
        bool stop();
        bool is_running() const { return _poll.is_running(); }

        bool socket_opened() { return _socket.is_open(); }
        const Socket & socket() const { return _socket; }
        const sihd::util::IClock *clock() const { return _clock_ptr; }

    protected:

    private:
        struct icmp *icmp();
        struct icmp6_hdr *icmp6();

        void handle(sihd::util::Poll *poll);
        void _setup_poll();
        void _read_socket();
        void _process_ipv4();
        void _process_ipv6();

        void _prepare_send();
        void _after_send();

        uint16_t _in_cksum(uint16_t *addr, int len);

        Socket _socket;
        std::mutex _poll_mutex;
        sihd::util::Poll _poll;
        sihd::util::ArrByte _array_rcv;
        sihd::util::ArrByte _array_send;
        sihd::util::IClock *_clock_ptr;

        IcmpResponse _icmp_response;
};

}

#endif
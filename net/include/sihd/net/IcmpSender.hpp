#ifndef __SIHD_NET_ICMPSENDER_HPP__
#define __SIHD_NET_ICMPSENDER_HPP__

#include <memory>

#include <sihd/util/ABlockingService.hpp>
#include <sihd/util/Configurable.hpp>
#include <sihd/util/Named.hpp>
#include <sihd/util/Poll.hpp>
#include <sihd/util/forward.hpp>

#include <sihd/net/Socket.hpp>
#include <sihd/net/ip.hpp>

struct icmp;
struct icmp6_hdr;

namespace sihd::net
{

struct IcmpResponse
{
        IpAddr client;
        void *data;
        size_t size;
        uint8_t type;
        uint8_t code;
        uint8_t ttl;
        uint16_t id;
        uint16_t seq;
};

class IcmpSender: public sihd::util::Named,
                  public sihd::util::Configurable,
                  public sihd::util::ABlockingService,
                  public sihd::util::Observable<IcmpSender>,
                  public sihd::util::IHandler<sihd::util::Poll *>
{
    public:
        IcmpSender(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~IcmpSender();

        bool open_socket(bool ipv6 = false);
        bool open_socket_unix();
        bool close();

        void set_echo();
        void set_type(int type);
        void set_code(int code);

        bool set_ttl(int ttl);
        void set_id(pid_t id);
        void set_seq(int seq);
        bool set_data(sihd::util::ArrByteView view);
        bool set_data_size(size_t byte_size);

        bool send_to(const IpAddr & addr);

        bool set_poll_timeout(int milliseconds);
        // poll for x milliseconds - returns true if socket is read
        bool poll(int milliseconds);
        // poll once with configured timeout
        bool poll();

        // in ms
        int poll_timeout() { return _poll.timeout(); }
        bool socket_opened() { return _socket.is_open(); }
        const Socket & socket() const { return _socket; }
        const IcmpResponse & response() const { return _icmp_response; }

    protected:
        void handle(sihd::util::Poll *poll) override;
        bool on_start() override;
        bool on_stop() override;

    private:
        struct icmp *icmp();
        struct icmp6_hdr *icmp6();

        void _setup_poll();
        void _read_socket();
        void _process_ipv4();
        void _process_ipv6();

        Socket _socket;
        std::mutex _poll_mutex;
        sihd::util::Poll _poll;
        std::unique_ptr<sihd::util::ArrByte> _array_rcv_ptr;
        std::unique_ptr<sihd::util::ArrByte> _array_send_ptr;

        IcmpResponse _icmp_response;
};

} // namespace sihd::net

#endif

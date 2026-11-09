#include <sihd/util/build.hpp>

#if defined(__SIHD_LINUX__)

# include <netinet/in.h>
# include <sys/socket.h>

# include <algorithm>
# include <map>
# include <random>
# include <set>
# include <variant>
# include <vector>

# include <sihd/net/Socket.hpp>
# include <sihd/net/linux/TunRelay.hpp>
# include <sihd/util/Clocks.hpp>
# include <sihd/util/Logger.hpp>
# include <sihd/util/Timestamp.hpp>
# include <sihd/util/time.hpp>

# include "ip_packet.hpp"

using namespace sihd::util;
using namespace sihd::sys;
using namespace sihd::net::linux_::ip_packet;

namespace sihd::net
{
SIHD_LOGGER;
}

namespace sihd::net::linux_
{

namespace
{

constexpr size_t default_mtu = 1500;
constexpr size_t min_mtu = 1280;
constexpr size_t max_mtu = 65535;
constexpr size_t ipv4_overhead = 40;
constexpr size_t ipv6_overhead = 60;
constexpr uint16_t relay_window = 65535;
constexpr uint16_t default_client_mss = 536;
constexpr int max_retransmits = 8;
constexpr int poll_tick_milliseconds = 100;
constexpr size_t read_buffer_size = 65535;

enum class TcpState
{
    connecting,
    syn_received,
    established,
};

struct UdpFlow
{
        Socket socket;
        Timestamp last_activity;
};

struct TcpFlow
{
        Socket socket;
        TcpState state = TcpState::connecting;

        uint32_t client_next = 0;
        uint32_t send_unacked = 0;
        uint32_t send_next = 0;
        uint16_t client_window = 0;
        uint16_t client_mss = default_client_mss;
        std::vector<uint8_t> unacked;

        bool reading = false;
        bool client_fin = false;
        bool server_eof = false;
        bool fin_sent = false;
        bool fin_acked = false;

        Timestamp last_activity;
        Timestamp retransmit_at;
        bool retransmit_armed = false;
        Duration backoff;
        int retransmits = 0;
};

using TcpFlows = std::map<FlowKey, TcpFlow>;
using UdpFlows = std::map<FlowKey, UdpFlow>;

bool seq_before(uint32_t lhs, uint32_t rhs)
{
    return static_cast<int32_t>(lhs - rhs) < 0;
}

Result<void> open_flow_socket(Socket & socket, const FlowKey & key, int type, int protocol)
{
    auto opened = socket.open(key.version == 4 ? AF_INET : AF_INET6, type, protocol);
    SIHD_UNEXPECTED_RETURN(opened);
    auto non_blocking = socket.set_blocking(false);
    SIHD_UNEXPECTED_RETURN(non_blocking);
    return socket.connect(destination(key));
}

} // namespace

struct TunRelay::Impl
{
        Impl(TunRelay & owner): relay(owner) {}

        TunRelay & relay;
        Tun tun;
        EventPoll poll;

        size_t mtu = default_mtu;
        Duration udp_timeout = Duration(time::seconds(60));
        Duration tcp_timeout = Duration(time::hours(1));
        Duration retransmit_timeout = Duration(time::seconds(1));

        UdpFlows udp_flows;
        TcpFlows tcp_flows;
        std::map<int, FlowKey> socket_flows;

        std::vector<uint8_t> read_buffer = std::vector<uint8_t>(read_buffer_size);
        std::vector<uint8_t> out;
        ArrUByteView packet;
        Direction direction = Direction::from_tun;

        SteadyClock clock;
        Timestamp last_sweep;
        bool tun_lost = false;
        int tun_fd = -1;
        std::set<int> erased_fds;
        std::mt19937 isn_generator {std::random_device {}()};

        void notify(ArrUByteView data, Direction dir);
        void write_tun();
        uint16_t relay_mss(const FlowKey & key) const;

        void send_segment(const FlowKey & key, const TcpSegment & segment);
        void send_control(const FlowKey & key, const TcpFlow & flow, uint32_t seq, uint8_t flags, uint16_t mss = 0);
        void send_ack(const FlowKey & key, const TcpFlow & flow);
        void send_syn_ack(const FlowKey & key, const TcpFlow & flow);
        void send_reset(const FlowKey & key, uint32_t seq, uint32_t ack);
        void send_data(const FlowKey & key, TcpFlow & flow, size_t offset, size_t size);
        void send_fin(const FlowKey & key, TcpFlow & flow, Timestamp now);

        void arm_retransmit(TcpFlow & flow, Timestamp now);
        void set_reading(TcpFlow & flow, bool reading);
        size_t window_room(const TcpFlow & flow) const;
        void establish(TcpFlow & flow);
        bool flow_done(const TcpFlow & flow) const;

        void watch(int fd, const FlowKey & key);
        void erase_tcp(TcpFlows::iterator it);
        void erase_udp(UdpFlows::iterator it);
        void reset_tcp(TcpFlows::iterator it);

        void tcp_open(const TcpPacket & packet, Timestamp now);
        void tcp_connected(TcpFlows::iterator it, Timestamp now);
        void tcp_acknowledge(TcpFlow & flow, const TcpSegment & segment, Timestamp now);
        bool tcp_forward_data(const FlowKey & key, TcpFlow & flow, const TcpSegment & segment);
        void tcp_receive_fin(TcpFlows::iterator it, const TcpSegment & segment);
        void tcp_from_tun(const TcpPacket & packet, Timestamp now);
        void tcp_server_eof(TcpFlows::iterator it, Timestamp now);
        void tcp_from_socket(TcpFlows::iterator it, Timestamp now);
        void tcp_socket_event(TcpFlows::iterator it, const PollEvent & event, Timestamp now);
        void tcp_retransmit(TcpFlows::iterator it, Timestamp now);

        void udp_from_tun(const UdpDatagram & datagram, Timestamp now);
        void udp_from_socket(UdpFlows::iterator it, Timestamp now);

        void read_tun(Timestamp now);
        void socket_event(const PollEvent & event, Timestamp now);
        void sweep(Timestamp now);
};

void TunRelay::Impl::notify(ArrUByteView data, Direction dir)
{
    packet = data;
    direction = dir;
    relay.notify_observers(&relay);
}

void TunRelay::Impl::write_tun()
{
    this->notify(ArrUByteView(out), Direction::to_tun);
    // a refused packet is lost (TCP retransmits), a tun down (revoked VPN) or closed ends the relay
    auto written = tun.write(ArrUByteView(out));
    if (!written && (written.error().code == ErrorCode::io_error || written.error().code == ErrorCode::closed))
        tun_lost = true;
}

uint16_t TunRelay::Impl::relay_mss(const FlowKey & key) const
{
    return static_cast<uint16_t>(mtu - (key.version == 4 ? ipv4_overhead : ipv6_overhead));
}

void TunRelay::Impl::send_segment(const FlowKey & key, const TcpSegment & segment)
{
    build_tcp_reply(out, key, segment);
    this->write_tun();
}

void TunRelay::Impl::send_control(const FlowKey & key, const TcpFlow & flow, uint32_t seq, uint8_t flags, uint16_t mss)
{
    this->send_segment(
        key,
        {.seq = seq, .ack = flow.client_next, .flags = flags, .window = relay_window, .mss = mss, .payload = {}});
}

void TunRelay::Impl::send_ack(const FlowKey & key, const TcpFlow & flow)
{
    this->send_control(key, flow, flow.send_next, tcp_ack);
}

void TunRelay::Impl::send_syn_ack(const FlowKey & key, const TcpFlow & flow)
{
    this->send_control(key, flow, flow.send_unacked, tcp_syn | tcp_ack, this->relay_mss(key));
}

void TunRelay::Impl::send_reset(const FlowKey & key, uint32_t seq, uint32_t ack)
{
    this->send_segment(key, {.seq = seq, .ack = ack, .flags = tcp_rst | tcp_ack, .window = 0, .mss = 0, .payload = {}});
}

void TunRelay::Impl::arm_retransmit(TcpFlow & flow, Timestamp now)
{
    if (flow.retransmit_armed)
        return;
    flow.retransmit_armed = true;
    flow.backoff = retransmit_timeout;
    flow.retransmit_at = now + flow.backoff;
}

void TunRelay::Impl::watch(int fd, const FlowKey & key)
{
    socket_flows[fd] = key;
}

void TunRelay::Impl::erase_tcp(TcpFlows::iterator it)
{
    erased_fds.insert(it->second.socket.socket());
    poll.clear_fd(it->second.socket.socket());
    socket_flows.erase(it->second.socket.socket());
    tcp_flows.erase(it);
}

void TunRelay::Impl::erase_udp(UdpFlows::iterator it)
{
    erased_fds.insert(it->second.socket.socket());
    poll.clear_fd(it->second.socket.socket());
    socket_flows.erase(it->second.socket.socket());
    udp_flows.erase(it);
}

void TunRelay::Impl::reset_tcp(TcpFlows::iterator it)
{
    this->send_reset(it->first, it->second.send_next, it->second.client_next);
    this->erase_tcp(it);
}

void TunRelay::Impl::send_data(const FlowKey & key, TcpFlow & flow, size_t offset, size_t size)
{
    const size_t end = offset + size;
    while (offset < end)
    {
        const size_t chunk = std::min<size_t>(end - offset, flow.client_mss);
        const bool last = offset + chunk == end;
        this->send_segment(key,
                           {.seq = flow.send_unacked + static_cast<uint32_t>(offset),
                            .ack = flow.client_next,
                            .flags = static_cast<uint8_t>(tcp_ack | (last ? tcp_psh : 0)),
                            .window = relay_window,
                            .mss = 0,
                            .payload = ArrUByteView(flow.unacked.data() + offset, chunk)});
        offset += chunk;
    }
}

void TunRelay::Impl::send_fin(const FlowKey & key, TcpFlow & flow, Timestamp now)
{
    this->send_control(key, flow, flow.send_next, tcp_fin | tcp_ack);
    flow.send_next += 1;
    flow.fin_sent = true;
    this->arm_retransmit(flow, now);
}

void TunRelay::Impl::set_reading(TcpFlow & flow, bool reading)
{
    if (flow.reading == reading)
        return;
    flow.reading = reading;
    if (reading)
        poll.set_read_fd(flow.socket.socket());
    else
        poll.rm_read_fd(flow.socket.socket());
}

size_t TunRelay::Impl::window_room(const TcpFlow & flow) const
{
    return flow.client_window > flow.unacked.size() ? flow.client_window - flow.unacked.size() : 0;
}

void TunRelay::Impl::establish(TcpFlow & flow)
{
    flow.state = TcpState::established;
    flow.retransmit_armed = false;
    flow.retransmits = 0;
}

bool TunRelay::Impl::flow_done(const TcpFlow & flow) const
{
    return flow.client_fin && flow.server_eof && flow.fin_acked && flow.unacked.empty();
}

void TunRelay::Impl::tcp_open(const TcpPacket & packet, Timestamp now)
{
    const FlowKey & key = packet.key;
    const TcpSegment & segment = packet.segment;
    TcpFlow flow;
    flow.client_next = segment.seq + 1;
    flow.client_window = segment.window;
    if (segment.mss != 0)
        flow.client_mss = std::min<uint16_t>(segment.mss, this->relay_mss(key));
    flow.send_unacked = static_cast<uint32_t>(isn_generator());
    flow.send_next = flow.send_unacked + 1;
    flow.last_activity = now;

    auto connected = open_flow_socket(flow.socket, key, SOCK_STREAM, IPPROTO_TCP);
    if (!connected && connected.error().code != ErrorCode::would_block)
    {
        this->send_reset(key, 0, flow.client_next);
        return;
    }

    const int fd = flow.socket.socket();
    auto [it, inserted] = tcp_flows.emplace(key, std::move(flow));
    this->watch(fd, key);
    if (connected)
        this->tcp_connected(it, now);
    else
        poll.set_write_fd(fd);
}

void TunRelay::Impl::tcp_connected(TcpFlows::iterator it, Timestamp now)
{
    TcpFlow & flow = it->second;
    flow.state = TcpState::syn_received;
    this->send_syn_ack(it->first, flow);
    this->arm_retransmit(flow, now);
}

void TunRelay::Impl::tcp_acknowledge(TcpFlow & flow, const TcpSegment & segment, Timestamp now)
{
    flow.client_window = segment.window;
    if ((segment.flags & tcp_ack) == 0)
        return;
    if (seq_before(flow.send_next, segment.ack) || !seq_before(flow.send_unacked, segment.ack))
        return;

    uint32_t acked = segment.ack - flow.send_unacked;
    if (flow.state == TcpState::syn_received)
    {
        acked -= 1;
        flow.send_unacked += 1;
        this->establish(flow);
    }
    const size_t data_acked = std::min<size_t>(acked, flow.unacked.size());
    flow.unacked.erase(flow.unacked.begin(), flow.unacked.begin() + static_cast<ptrdiff_t>(data_acked));
    flow.send_unacked = segment.ack;
    if (flow.fin_sent && segment.ack == flow.send_next)
        flow.fin_acked = true;

    flow.retransmits = 0;
    flow.retransmit_armed = false;
    if (!flow.unacked.empty() || (flow.fin_sent && !flow.fin_acked))
        this->arm_retransmit(flow, now);
}

bool TunRelay::Impl::tcp_forward_data(const FlowKey & key, TcpFlow & flow, const TcpSegment & segment)
{
    if (segment.payload.size() == 0)
        return true;
    if (segment.seq != flow.client_next || flow.client_fin)
    {
        this->send_ack(key, flow);
        return true;
    }
    auto sent = flow.socket.send(ArrCharView(segment.payload.data(), segment.payload.size()));
    if (!sent && sent.error().code != ErrorCode::would_block)
        return false;

    // unsent bytes stay unacknowledged: the client retransmits them
    flow.client_next += static_cast<uint32_t>(sent.value_or(0));
    this->send_ack(key, flow);
    return true;
}

void TunRelay::Impl::tcp_receive_fin(TcpFlows::iterator it, const TcpSegment & segment)
{
    TcpFlow & flow = it->second;
    const uint32_t fin_seq = segment.seq + static_cast<uint32_t>(segment.payload.size());
    if (flow.client_fin || fin_seq != flow.client_next)
        return;
    flow.client_fin = true;
    flow.client_next += 1;
    (void)flow.socket.shutdown_send();
    this->send_ack(it->first, flow);
}

void TunRelay::Impl::tcp_from_tun(const TcpPacket & packet, Timestamp now)
{
    const TcpSegment & segment = packet.segment;
    auto it = tcp_flows.find(packet.key);
    if (it == tcp_flows.end())
    {
        if ((segment.flags & (tcp_syn | tcp_ack | tcp_rst)) == tcp_syn)
            this->tcp_open(packet, now);
        else if ((segment.flags & tcp_rst) == 0)
            this->send_reset(packet.key, segment.ack, segment.seq + static_cast<uint32_t>(segment.payload.size()));
        return;
    }

    TcpFlow & flow = it->second;
    flow.last_activity = now;
    if (segment.flags & tcp_rst)
    {
        this->erase_tcp(it);
        return;
    }
    if (segment.flags & tcp_syn)
    {
        if (flow.state == TcpState::syn_received)
            this->send_syn_ack(it->first, flow);
        return;
    }
    if (flow.state == TcpState::connecting)
        return;

    this->tcp_acknowledge(flow, segment, now);
    if (!this->tcp_forward_data(it->first, flow, segment))
    {
        this->reset_tcp(it);
        return;
    }
    if (segment.flags & tcp_fin)
        this->tcp_receive_fin(it, segment);

    if (this->flow_done(flow))
    {
        this->erase_tcp(it);
        return;
    }
    if (flow.state == TcpState::established && !flow.server_eof)
    {
        this->set_reading(flow, this->window_room(flow) > 0);
        if (flow.client_window == 0)
            this->arm_retransmit(flow, now);
    }
}

void TunRelay::Impl::tcp_server_eof(TcpFlows::iterator it, Timestamp now)
{
    TcpFlow & flow = it->second;
    flow.server_eof = true;
    this->set_reading(flow, false);
    this->send_fin(it->first, flow, now);
}

void TunRelay::Impl::tcp_from_socket(TcpFlows::iterator it, Timestamp now)
{
    TcpFlow & flow = it->second;
    const size_t room = std::min(this->window_room(flow), read_buffer.size());
    if (room == 0)
    {
        this->set_reading(flow, false);
        return;
    }

    auto received = flow.socket.receive(read_buffer.data(), room);
    if (!received && received.error().code == ErrorCode::would_block)
        return;
    if (!received)
    {
        this->reset_tcp(it);
        return;
    }
    flow.last_activity = now;
    if (*received == 0)
    {
        this->tcp_server_eof(it, now);
        return;
    }

    const size_t offset = flow.unacked.size();
    flow.unacked.insert(flow.unacked.end(),
                        read_buffer.begin(),
                        read_buffer.begin() + static_cast<ptrdiff_t>(*received));
    flow.send_next += static_cast<uint32_t>(*received);
    this->send_data(it->first, flow, offset, *received);
    this->arm_retransmit(flow, now);
    if (this->window_room(flow) == 0)
        this->set_reading(flow, false);
}

void TunRelay::Impl::tcp_socket_event(TcpFlows::iterator it, const PollEvent & event, Timestamp now)
{
    TcpFlow & flow = it->second;
    if (flow.state == TcpState::connecting)
    {
        if (!event.writable && !event.error && !event.closed)
            return;
        poll.rm_write_fd(event.fd);
        auto error = flow.socket.get_error();
        if (!error || *error != 0)
        {
            this->send_reset(it->first, 0, flow.client_next);
            this->erase_tcp(it);
            return;
        }
        this->tcp_connected(it, now);
        return;
    }
    if (event.readable || event.closed || event.error)
        this->tcp_from_socket(it, now);
}

void TunRelay::Impl::tcp_retransmit(TcpFlows::iterator it, Timestamp now)
{
    TcpFlow & flow = it->second;
    const bool fin_pending = flow.fin_sent && !flow.fin_acked;
    const bool outstanding = flow.state == TcpState::syn_received || !flow.unacked.empty() || fin_pending;
    if (!outstanding && flow.client_window != 0)
    {
        flow.retransmit_armed = false;
        return;
    }
    if (outstanding && flow.retransmits >= max_retransmits)
    {
        this->reset_tcp(it);
        return;
    }
    if (outstanding)
        flow.retransmits += 1;
    flow.backoff = flow.backoff * 2;
    flow.retransmit_at = now + flow.backoff;

    if (flow.state == TcpState::syn_received)
    {
        this->send_syn_ack(it->first, flow);
        return;
    }
    if (!flow.unacked.empty())
        this->send_data(it->first,
                        flow,
                        0,
                        std::min<size_t>(flow.unacked.size(), std::max<size_t>(flow.client_window, flow.client_mss)));
    if (fin_pending)
        this->send_control(it->first, flow, flow.send_next - 1, tcp_fin | tcp_ack);
    // a closed window is reopened by an ACK the client may never resend: probe it until tcp_timeout
    if (!outstanding)
        this->send_control(it->first, flow, flow.send_next - 1, tcp_ack);
}

void TunRelay::Impl::udp_from_tun(const UdpDatagram & datagram, Timestamp now)
{
    auto it = udp_flows.find(datagram.key);
    if (it == udp_flows.end())
    {
        UdpFlow flow;
        if (!open_flow_socket(flow.socket, datagram.key, SOCK_DGRAM, IPPROTO_UDP))
            return;
        const int fd = flow.socket.socket();
        it = udp_flows.emplace(datagram.key, std::move(flow)).first;
        this->watch(fd, datagram.key);
        poll.set_read_fd(fd);
    }
    it->second.last_activity = now;
    (void)it->second.socket.send(ArrCharView(datagram.payload.data(), datagram.payload.size()));
}

void TunRelay::Impl::udp_from_socket(UdpFlows::iterator it, Timestamp now)
{
    while (true)
    {
        auto received = it->second.socket.receive(read_buffer.data(), read_buffer.size());
        if (!received)
            return;
        it->second.last_activity = now;
        build_udp_reply(out, it->first, ArrUByteView(read_buffer.data(), *received));
        this->write_tun();
    }
}

void TunRelay::Impl::read_tun(Timestamp now)
{
    while (!tun_lost)
    {
        auto read = tun.read(read_buffer.data(), read_buffer.size());
        if (!read && read.error().code == ErrorCode::would_block)
            return;
        if (!read || *read == 0)
        {
            tun_lost = true;
            return;
        }
        const ArrUByteView data(read_buffer.data(), *read);
        this->notify(data, Direction::from_tun);
        auto parsed = ip_packet::parse(data);
        if (!parsed)
            continue;
        if (const auto *tcp = std::get_if<TcpPacket>(&*parsed))
            this->tcp_from_tun(*tcp, now);
        else
            this->udp_from_tun(std::get<UdpDatagram>(*parsed), now);
    }
}

void TunRelay::Impl::socket_event(const PollEvent & event, Timestamp now)
{
    // a flow erased earlier in this poll frees its fd for a new flow: its stale events are dropped
    auto flow_key = socket_flows.find(event.fd);
    if (flow_key == socket_flows.end() || erased_fds.contains(event.fd))
        return;
    if (flow_key->second.protocol == IPPROTO_TCP)
    {
        auto it = tcp_flows.find(flow_key->second);
        if (it != tcp_flows.end())
            this->tcp_socket_event(it, event, now);
        return;
    }
    auto it = udp_flows.find(flow_key->second);
    if (it != udp_flows.end())
        this->udp_from_socket(it, now);
}

void TunRelay::Impl::sweep(Timestamp now)
{
    for (auto it = udp_flows.begin(); it != udp_flows.end();)
    {
        auto current = it++;
        if (now - current->second.last_activity >= udp_timeout)
            this->erase_udp(current);
    }
    for (auto it = tcp_flows.begin(); it != tcp_flows.end();)
    {
        auto current = it++;
        TcpFlow & flow = current->second;
        if (now - flow.last_activity >= tcp_timeout)
            this->reset_tcp(current);
        else if (flow.retransmit_armed && now >= flow.retransmit_at)
            this->tcp_retransmit(current, now);
    }
}

TunRelay::TunRelay(const std::string & name, Node *parent): Named(name, parent), _impl(std::make_unique<Impl>(*this))
{
    _impl->poll.set_timeout(poll_tick_milliseconds);
    _impl->poll.add_observer(this);
}

TunRelay::~TunRelay()
{
    if (this->is_running())
        this->stop();
    _impl->poll.remove_observer(this);
}

bool TunRelay::set_tun(Tun && tun)
{
    if (this->is_running())
        return false;
    _impl->tun = std::move(tun);
    return true;
}

bool TunRelay::set_mtu(size_t mtu)
{
    if (this->is_running() || mtu < min_mtu || mtu > max_mtu)
        return false;
    _impl->mtu = mtu;
    return true;
}

bool TunRelay::set_udp_timeout(Duration timeout)
{
    if (this->is_running() || timeout <= Duration(0))
        return false;
    _impl->udp_timeout = timeout;
    return true;
}

bool TunRelay::set_tcp_timeout(Duration timeout)
{
    if (this->is_running() || timeout <= Duration(0))
        return false;
    _impl->tcp_timeout = timeout;
    return true;
}

bool TunRelay::set_retransmit_timeout(Duration timeout)
{
    if (this->is_running() || timeout <= Duration(0))
        return false;
    _impl->retransmit_timeout = timeout;
    return true;
}

ArrUByteView TunRelay::packet() const
{
    return _impl->packet;
}

TunRelay::Direction TunRelay::direction() const
{
    return _impl->direction;
}

void TunRelay::handle(EventPoll *poll)
{
    if (this->service_relay_stop(poll))
        return;
    const Timestamp now = _impl->clock.now();
    _impl->erased_fds.clear();
    for (const PollEvent & event : poll->events())
    {
        if (event.fd == _impl->tun_fd)
            _impl->read_tun(now);
        else
            _impl->socket_event(event, now);
    }
    if (now - _impl->last_sweep >= Duration(time::milli(poll_tick_milliseconds)))
    {
        _impl->last_sweep = now;
        _impl->sweep(now);
    }
    if (_impl->tun_lost)
        poll->request_stop();
}

// ready once the poll runs: a stop before would find it stopped and be lost
bool TunRelay::is_ready() const
{
    return _impl->poll.is_ready();
}

bool TunRelay::wait_ready(Duration timeout) const
{
    return _impl->poll.wait_ready(timeout);
}

bool TunRelay::on_start()
{
    auto tun_fd = _impl->tun.fd();
    if (!tun_fd || !_impl->poll.set_read_fd(*tun_fd))
    {
        SIHD_LOG(error, "TunRelay: no tun to relay");
        return false;
    }
    _impl->tun_fd = *tun_fd;
    _impl->tun_lost = false;
    SIHD_LOG(debug, "TunRelay: started");
    const bool ran = _impl->poll.start();

    _impl->poll.clear_fds();
    _impl->tcp_flows.clear();
    _impl->udp_flows.clear();
    _impl->socket_flows.clear();
    SIHD_LOG(debug, "TunRelay: stopped");
    return ran;
}

bool TunRelay::on_stop()
{
    return _impl->poll.stop();
}

} // namespace sihd::net::linux_

#endif

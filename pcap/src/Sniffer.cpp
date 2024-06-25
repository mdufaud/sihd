#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/time.hpp>

#include <sihd/pcap/Sniffer.hpp>

namespace sihd::pcap
{

SIHD_UTIL_REGISTER_FACTORY(Sniffer)

SIHD_LOGGER;

Sniffer::Sniffer(const std::string & name, sihd::util::Node *parent):
    sihd::util::Named(name, parent),
    _active(false),
    _running(false),
    _max_sniff(-1),
    _timestamp_type(-1),
    _nano_precision(false),
    _pcap_ptr(nullptr),
    _pkt_nano_timestamp(0)
{
    utils::init();
    this->add_conf("max_sniff", &Sniffer::set_max_sniff);
    this->add_conf("linux_protocol", &Sniffer::set_linux_protocol);
    this->add_conf("promiscuous_mode", &Sniffer::set_promiscuous);
    this->add_conf("monitor_mode", &Sniffer::set_monitor);
    this->add_conf("immediate_mode", &Sniffer::set_immediate);
    this->add_conf("timestamp_nano", &Sniffer::set_timestamp_nano);
    this->add_conf("nonblock", &Sniffer::set_nonblock);
    this->add_conf("datalink", &Sniffer::set_datalink);
    this->add_conf("timestamp_type", &Sniffer::set_timestamp_type);
    this->add_conf("buffer_size", &Sniffer::set_buffer_size);
    this->add_conf("snaplen", &Sniffer::set_snaplen);
    this->add_conf("timeout", &Sniffer::set_timeout);
    this->add_conf("direction", &Sniffer::set_direction);
    this->add_conf("filter", &Sniffer::set_filter);
}

Sniffer::~Sniffer()
{
    if (this->is_running())
        this->stop();
    this->close();
}

bool Sniffer::open(const std::string & source)
{
    char errbuf[PCAP_ERRBUF_SIZE];
    _pcap_ptr = pcap_create(source.c_str(), errbuf);
    if (_pcap_ptr == nullptr)
        SIHD_LOG(error, "Sniffer: {}", errbuf);
    return _pcap_ptr != nullptr;
}

bool Sniffer::is_open() const
{
    return _pcap_ptr != nullptr;
}

bool Sniffer::close()
{
    if (_pcap_ptr != nullptr)
    {
        pcap_close(_pcap_ptr);
        _pcap_ptr = nullptr;
    }
    _active = false;
    _running = false;
    _nano_precision = false;
    return true;
}

bool Sniffer::is_active() const
{
    return _active;
}

bool Sniffer::activate()
{
    int ret = pcap_activate(_pcap_ptr);
    this->_log_if_error(ret);
    if (ret < 0)
        this->close();
    _active = ret == 0;
    return ret == 0;
}

void Sniffer::_callback(u_char *user, const struct pcap_pkthdr *h, const u_char *bytes)
{
    Sniffer *obj = (Sniffer *)user;
    obj->new_packet(h, bytes);
}

bool Sniffer::on_start()
{
    _running = true;
    int ret = pcap_loop(_pcap_ptr, _max_sniff, Sniffer::_callback, (u_char *)this);
    if (ret == PCAP_ERROR)
    {
        SIHD_LOG(error, "Sniffer: {}", this->error());
    }
    else if (ret == PCAP_ERROR_BREAK)
    {
        SIHD_LOG(warning, "Sniffer: broke loop before the total packet count was read");
    }
    _running = false;
    return ret == 0;
}

bool Sniffer::is_running() const
{
    return _running;
}

bool Sniffer::on_stop()
{
    pcap_breakloop(_pcap_ptr);
    _running = false;
    return true;
}

bool Sniffer::sniff()
{
    int ret = pcap_dispatch(_pcap_ptr, _max_sniff, Sniffer::_callback, (u_char *)this);
    if (ret == PCAP_ERROR)
    {
        SIHD_LOG(error, "Sniffer: {}", this->error());
    }
    else if (ret == PCAP_ERROR_BREAK)
    {
        SIHD_LOG(warning, "Sniffer: broke dispatch before the total packet count was read");
    }
    return ret == 0;
}

bool Sniffer::read_next()
{
    struct pcap_pkthdr *hdr;
    u_char *data;
    int ret = pcap_next_ex(_pcap_ptr, &hdr, (const u_char **)(&data));
    if (ret == PCAP_ERROR)
    {
        SIHD_LOG(error, "Sniffer: {}", this->error());
    }
    else if (ret >= 0)
        this->new_packet(hdr, data);
    return ret == 0;
}

// data retrieval

void Sniffer::new_packet(const struct pcap_pkthdr *h, const u_char *bytes)
{
    if (_nano_precision)
        _pkt_nano_timestamp = sihd::util::time::nano_tv(h->ts);
    else
        _pkt_nano_timestamp = sihd::util::time::tv(h->ts);
    _array.resize(h->len);
    _array.copy_from_bytes(bytes, h->len);
    this->notify_observers(this);
}

const sihd::util::ArrByte & Sniffer::data() const
{
    return _array;
}

bool Sniffer::get_read_data(sihd::util::ArrCharView & view) const
{
    view = {_array.buf(), _array.byte_size()};
    return true;
}

bool Sniffer::get_read_timestamp(time_t *nano_timestamp) const
{
    *nano_timestamp = _pkt_nano_timestamp;
    return true;
}

// polling utilities

int Sniffer::pollable_fd()
{
#if !defined(__SIHD_WINDOWS__)
    return pcap_get_selectable_fd(_pcap_ptr);
#else
    return -1;
#endif
}

const struct timeval *Sniffer::poll_timeout()
{
#if !defined(__SIHD_WINDOWS__)
    return pcap_get_required_select_timeout(_pcap_ptr);
#else
    return nullptr;
#endif
}

const struct pcap_stat *Sniffer::stats()
{
    if (pcap_stats(_pcap_ptr, &_pcap_stats) == 0)
        return &_pcap_stats;
    this->_log_if_error(PCAP_ERROR);
    return nullptr;
}

// error utilities

std::string Sniffer::error()
{
    return pcap_geterr(_pcap_ptr);
}

void Sniffer::_log_if_error(int ret)
{
    if (ret == PCAP_ERROR)
    {
        SIHD_LOG(error, "Sniffer: {}", this->error());
    }
    else if (ret == PCAP_WARNING)
    {
        SIHD_LOG(warning, "Sniffer: {}", this->error());
    }
    else if (ret != 0)
    {
        SIHD_LOG(error, "Sniffer: {}", utils::status_str(ret));
    }
}

// helpers

std::vector<int> Sniffer::datalinks()
{
    std::vector<int> ret;
    int *lst = nullptr;
    int n = pcap_list_datalinks(_pcap_ptr, &lst);
    if (n >= 0)
    {
        ret.reserve(n);
        int i = 0;
        while (i < n)
        {
            ret[i] = lst[i];
            ++i;
        }
        pcap_free_datalinks(lst);
    }
    else
        SIHD_LOG(error, "Sniffer: {}", this->error());
    return ret;
}

std::vector<int> Sniffer::timestamp_types()
{
    std::vector<int> ret;
    int *lst = nullptr;
    int n = pcap_list_tstamp_types(_pcap_ptr, &lst);
    if (n >= 0)
    {
        ret.reserve(n);
        int i = 0;
        while (i < n)
        {
            ret[i] = lst[i];
            ++i;
        }
        pcap_free_tstamp_types(lst);
    }
    else
        SIHD_LOG(error, "Sniffer: {}", this->error());
    return ret;
}

// getters

bool Sniffer::can_monitor()
{
    int ret = pcap_can_set_rfmon(_pcap_ptr);
    this->_log_if_error(ret);
    return ret == 0;
}

bool Sniffer::is_nonblock()
{
    char errbuf[PCAP_ERRBUF_SIZE];
    int ret = pcap_getnonblock(_pcap_ptr, errbuf);
    if (ret < 0)
        SIHD_LOG(error, "Sniffer: {}", errbuf);
    return ret == 1;
}

bool Sniffer::timestamp_nano()
{
    return pcap_get_tstamp_precision(_pcap_ptr) == PCAP_TSTAMP_PRECISION_NANO;
}

bool Sniffer::timestamp_micro()
{
    return pcap_get_tstamp_precision(_pcap_ptr) == PCAP_TSTAMP_PRECISION_MICRO;
}

int Sniffer::timestamp_type()
{
    return _timestamp_type;
}

int Sniffer::snaplen()
{
    return pcap_snapshot(_pcap_ptr);
}

int Sniffer::datalink()
{
    return pcap_datalink(_pcap_ptr);
}

// settings

bool Sniffer::set_filter(std::string_view filter)
{
    bpf_u_int32 netmask = PCAP_NETMASK_UNKNOWN;
    // bpf_u_int32 netmask = 0;
    bool optimize = true;
    struct bpf_program pcap_filter;
    int ret = pcap_compile(_pcap_ptr, &pcap_filter, filter.data(), (int)optimize, netmask);
    if (ret == 0)
        ret = pcap_setfilter(_pcap_ptr, &pcap_filter);
    this->_log_if_error(ret);
    return ret == 0;
}

bool Sniffer::set_direction(std::string_view direction)
{
    pcap_direction_t dir;
    if (direction == "in")
        dir = PCAP_D_IN;
    else if (direction == "out")
        dir = PCAP_D_OUT;
    else if (direction == "both")
        dir = PCAP_D_INOUT;
    else
    {
        SIHD_LOG(error, "Sniffer: packet direction unknown: '{}' possible values are: in - out - both", direction);
        return false;
    }
    int ret = pcap_setdirection(_pcap_ptr, dir);
    if (ret != 0)
        SIHD_LOG(error, "Sniffer: {}", this->error());
    return ret == 0;
}

bool Sniffer::set_max_sniff(size_t n)
{
    _max_sniff = n;
    return true;
}

bool Sniffer::set_linux_protocol(int protocol)
{
#if defined(__SIHD_LINUX__) && !defined(__SIHD_EMSCRIPTEN__)
    int ret = pcap_set_protocol_linux(_pcap_ptr, protocol);
    if (ret != 0)
        SIHD_LOG(error, "Sniffer: cannot set linux protocol on an activated capture handle");
    return ret == 0;
#else
    (void)protocol;
    SIHD_LOG(error, "Sniffer: cannot set linux protocol");
    return false;
#endif
}

bool Sniffer::set_promiscuous(bool active)
{
    int ret = pcap_set_promisc(_pcap_ptr, (int)active);
    if (ret != 0)
        SIHD_LOG(error, "Sniffer: cannot set promiscuous mode on an activated capture handle");
    return ret == 0;
}

bool Sniffer::set_monitor(bool active)
{
    int ret = pcap_set_rfmon(_pcap_ptr, (int)active);
    if (ret != 0)
        SIHD_LOG(error, "Sniffer: cannot set monitor mode on an activated capture handle");
    return ret == 0;
}

bool Sniffer::set_immediate(bool active)
{
    int ret = pcap_set_immediate_mode(_pcap_ptr, (int)active);
    if (ret != 0)
        SIHD_LOG(error, "Sniffer: cannot set immediate mode on an activated capture handle");
    return ret == 0;
}

bool Sniffer::set_buffer_size(int size)
{
    int ret = pcap_set_buffer_size(_pcap_ptr, size);
    if (ret != 0)
        SIHD_LOG(error, "Sniffer: cannot set buffer size on an activated capture handle");
    return ret == 0;
}

bool Sniffer::set_datalink(int datalink)
{
    int ret = pcap_set_datalink(_pcap_ptr, datalink);
    this->_log_if_error(ret);
    return ret == 0;
}

bool Sniffer::set_timestamp_type(int ts_type)
{
    int ret = pcap_set_tstamp_type(_pcap_ptr, ts_type);
    this->_log_if_error(ret);
    return ret == 0;
}

bool Sniffer::set_timestamp_nano(bool active)
{
    int ret = pcap_set_tstamp_precision(_pcap_ptr, active ? PCAP_TSTAMP_PRECISION_NANO : PCAP_TSTAMP_PRECISION_MICRO);
    this->_log_if_error(ret);
    _nano_precision = ret == 0 && active;
    return ret == 0;
}

bool Sniffer::set_snaplen(int len)
{
    int ret = pcap_set_snaplen(_pcap_ptr, len);
    if (ret != 0)
        SIHD_LOG(error, "Sniffer: cannot set snaplen on an activated capture handle");
    return ret == 0;
}

bool Sniffer::set_timeout(int ms)
{
    int ret = pcap_set_timeout(_pcap_ptr, ms);
    if (ret != 0)
        SIHD_LOG(error, "Sniffer: cannot set timeout on an activated capture handle");
    return ret == 0;
}

bool Sniffer::set_nonblock(bool block)
{
    char errbuf[PCAP_ERRBUF_SIZE];
    int ret = pcap_setnonblock(_pcap_ptr, (int)block, errbuf);
    if (ret != 0)
        SIHD_LOG(error, "Sniffer: {}", errbuf);
    return ret == 0;
}

} // namespace sihd::pcap

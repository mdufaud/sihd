#include <pcap.h>

#include <sihd/pcap/Sniffer.hpp>
#include <sihd/pcap/utils.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/time.hpp>

using namespace sihd::util;
using enum sihd::util::ErrorCode;

namespace sihd::pcap
{

struct Sniffer::Impl
{
        pcap_t *pcap_ptr {nullptr};
        struct pcap_stat pcap_stats {};

        static void callback(u_char *user, const struct pcap_pkthdr *h, const u_char *bytes)
        {
            auto *obj = reinterpret_cast<Sniffer *>(user);
            obj->_new_packet(h, bytes);
        }
};

SIHD_REGISTER_FACTORY(Sniffer)

SIHD_LOGGER;

Sniffer::Sniffer(const std::string & name, Node *parent):
    Named(name, parent),
    _impl_ptr(std::make_unique<Impl>()),
    _active(false),
    _max_sniff(-1),
    _timestamp_type(-1),
    _nano_precision(false),
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

Result<void> Sniffer::open(ZStringView source)
{
    char errbuf[PCAP_ERRBUF_SIZE];
    _impl_ptr->pcap_ptr = pcap_create(source.c_str(), errbuf);
    if (_impl_ptr->pcap_ptr == nullptr)
        return std::unexpected(Error(ErrorCode::io_error, "cannot create capture on '{}': {}", source, errbuf));
    return {};
}

bool Sniffer::is_open() const
{
    return _impl_ptr->pcap_ptr != nullptr;
}

bool Sniffer::close()
{
    if (_impl_ptr->pcap_ptr != nullptr)
    {
        pcap_close(_impl_ptr->pcap_ptr);
        _impl_ptr->pcap_ptr = nullptr;
    }
    _active = false;
    _nano_precision = false;
    return true;
}

bool Sniffer::is_active() const
{
    return _active;
}

Result<void> Sniffer::activate()
{
    int ret = pcap_activate(_impl_ptr->pcap_ptr);
    if (ret == PCAP_ERROR)
    {
        auto error = Error(ErrorCode::io_error, "cannot activate capture: {}", this->error());
        this->close();
        return std::unexpected(std::move(error));
    }
    // positive return: activation succeeded with a warning (promisc not supported, ...)
    if (ret > 0)
        SIHD_LOG(warning, "Sniffer: {}", utils::status_str(ret));
    _active = ret == 0;
    if (ret < 0)
    {
        this->close();
        return std::unexpected(Error(ErrorCode::io_error, "cannot activate capture: {}", utils::status_str(ret)));
    }
    return {};
}

bool Sniffer::on_start()
{
    this->service_set_ready();
    int ret = pcap_loop(_impl_ptr->pcap_ptr, _max_sniff, Sniffer::Impl::callback, (u_char *)this);
    if (ret == PCAP_ERROR)
    {
        SIHD_LOG(error, "Sniffer: {}", this->error());
    }
    else if (ret == PCAP_ERROR_BREAK)
    {
        SIHD_LOG(warning, "Sniffer: broke loop before the total packet count was read");
    }
    return ret == 0;
}

bool Sniffer::on_stop()
{
    pcap_breakloop(_impl_ptr->pcap_ptr);
    return true;
}

bool Sniffer::sniff()
{
    int ret = pcap_dispatch(_impl_ptr->pcap_ptr, _max_sniff, Sniffer::Impl::callback, (u_char *)this);
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

Result<bool> Sniffer::read_next()
{
    if (this->is_open() == false)
        return std::unexpected(Error(ErrorCode::not_initialized, "not open"));
    struct pcap_pkthdr *hdr;
    u_char *data;
    int ret = pcap_next_ex(_impl_ptr->pcap_ptr, &hdr, (const u_char **)(&data));
    if (ret == PCAP_ERROR)
        return std::unexpected(Error(io_error, "{}", this->error()));
    if (ret > 0)
        this->_new_packet(hdr, data);
    return ret > 0;
}

Result<size_t> Sniffer::inject(ArrCharView frame)
{
    if (this->is_open() == false)
        return std::unexpected(Error(not_initialized, "not open"));
    int ret = pcap_inject(_impl_ptr->pcap_ptr, frame.data(), frame.size());
    if (ret == PCAP_ERROR_NOT_ACTIVATED)
        return std::unexpected(Error(not_initialized, "capture not activated"));
    if (ret < 0)
        return std::unexpected(Error(io_error, "{}", this->error()));
    return static_cast<size_t>(ret);
}

void Sniffer::_new_packet(const void *hdr, const void *bytes)
{
    const auto *pkt_hdr = static_cast<const struct pcap_pkthdr *>(hdr);
    const auto *data = static_cast<const u_char *>(bytes);
    if (_nano_precision)
        _pkt_nano_timestamp = Timestamp(time::nano_tv(pkt_hdr->ts));
    else
        _pkt_nano_timestamp = Timestamp(time::tv(pkt_hdr->ts));
    _array.resize(pkt_hdr->len);
    _array.copy_from_bytes(data, pkt_hdr->len);
    this->notify_observers(this);
}

const ArrByte & Sniffer::data() const
{
    return _array;
}

const ArrByte & Sniffer::array() const
{
    return _array;
}

bool Sniffer::get_read_data(ArrCharView & view) const
{
    view = {_array.buf(), _array.byte_size()};
    return true;
}

Result<Timestamp> Sniffer::get_read_timestamp() const
{
    return _pkt_nano_timestamp;
}

Result<int> Sniffer::pollable_fd()
{
#if !defined(__SIHD_WINDOWS__)
    const int fd = pcap_get_selectable_fd(_impl_ptr->pcap_ptr);
    if (fd < 0)
        return std::unexpected(Error(not_found, "no selectable fd"));
    return fd;
#else
    return std::unexpected(Error(not_supported, "no selectable fd on windows"));
#endif
}

std::optional<Sniffer::Stats> Sniffer::stats()
{
    if (pcap_stats(_impl_ptr->pcap_ptr, &_impl_ptr->pcap_stats) == 0)
    {
        return Stats {
            .received = _impl_ptr->pcap_stats.ps_recv,
            .dropped = _impl_ptr->pcap_stats.ps_drop,
            .if_dropped = _impl_ptr->pcap_stats.ps_ifdrop,
        };
    }
    this->_log_if_error(PCAP_ERROR);
    return std::nullopt;
}

std::string Sniffer::error()
{
    if (_impl_ptr->pcap_ptr == nullptr)
        return {};
    return pcap_geterr(_impl_ptr->pcap_ptr);
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

std::vector<int> Sniffer::datalinks()
{
    std::vector<int> ret;
    int *list = nullptr;
    int count = pcap_list_datalinks(_impl_ptr->pcap_ptr, &list);
    if (count >= 0)
    {
        ret.reserve(count);
        for (int i = 0; i < count; ++i)
            ret.push_back(list[i]);
        pcap_free_datalinks(list);
    }
    else
        SIHD_LOG(error, "Sniffer: {}", this->error());
    return ret;
}

std::vector<int> Sniffer::timestamp_types()
{
    std::vector<int> ret;
    int *list = nullptr;
    int count = pcap_list_tstamp_types(_impl_ptr->pcap_ptr, &list);
    if (count >= 0)
    {
        ret.reserve(count);
        for (int i = 0; i < count; ++i)
            ret.push_back(list[i]);
        pcap_free_tstamp_types(list);
    }
    else
        SIHD_LOG(error, "Sniffer: {}", this->error());
    return ret;
}

bool Sniffer::can_monitor()
{
    int ret = pcap_can_set_rfmon(_impl_ptr->pcap_ptr);
    this->_log_if_error(ret);
    return ret == 0;
}

bool Sniffer::is_nonblock()
{
    char errbuf[PCAP_ERRBUF_SIZE];
    int ret = pcap_getnonblock(_impl_ptr->pcap_ptr, errbuf);
    if (ret < 0)
        SIHD_LOG(error, "Sniffer: {}", errbuf);
    return ret == 1;
}

bool Sniffer::timestamp_nano()
{
    return pcap_get_tstamp_precision(_impl_ptr->pcap_ptr) == PCAP_TSTAMP_PRECISION_NANO;
}

bool Sniffer::timestamp_micro()
{
    return pcap_get_tstamp_precision(_impl_ptr->pcap_ptr) == PCAP_TSTAMP_PRECISION_MICRO;
}

int Sniffer::timestamp_type()
{
    return _timestamp_type;
}

int Sniffer::snaplen()
{
    return pcap_snapshot(_impl_ptr->pcap_ptr);
}

int Sniffer::datalink()
{
    return pcap_datalink(_impl_ptr->pcap_ptr);
}

bool Sniffer::set_filter(ZStringView filter)
{
    bpf_u_int32 netmask = PCAP_NETMASK_UNKNOWN;
    bool optimize = true;
    struct bpf_program pcap_filter;
    int ret = pcap_compile(_impl_ptr->pcap_ptr, &pcap_filter, filter.c_str(), (int)optimize, netmask);
    if (ret == 0)
        ret = pcap_setfilter(_impl_ptr->pcap_ptr, &pcap_filter);
    pcap_freecode(&pcap_filter);
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
    int ret = pcap_setdirection(_impl_ptr->pcap_ptr, dir);
    if (ret != 0)
        SIHD_LOG(error, "Sniffer: {}", this->error());
    return ret == 0;
}

bool Sniffer::set_max_sniff(size_t max)
{
    _max_sniff = max;
    return true;
}

bool Sniffer::set_linux_protocol(int protocol)
{
#if defined(__SIHD_LINUX__) && !defined(__SIHD_EMSCRIPTEN__)
    int ret = pcap_set_protocol_linux(_impl_ptr->pcap_ptr, protocol);
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
    int ret = pcap_set_promisc(_impl_ptr->pcap_ptr, (int)active);
    if (ret != 0)
        SIHD_LOG(error, "Sniffer: cannot set promiscuous mode on an activated capture handle");
    return ret == 0;
}

bool Sniffer::set_monitor(bool active)
{
    int ret = pcap_set_rfmon(_impl_ptr->pcap_ptr, (int)active);
    if (ret != 0)
        SIHD_LOG(error, "Sniffer: cannot set monitor mode on an activated capture handle");
    return ret == 0;
}

bool Sniffer::set_immediate(bool active)
{
    int ret = pcap_set_immediate_mode(_impl_ptr->pcap_ptr, (int)active);
    if (ret != 0)
        SIHD_LOG(error, "Sniffer: cannot set immediate mode on an activated capture handle");
    return ret == 0;
}

bool Sniffer::set_buffer_size(int size)
{
    int ret = pcap_set_buffer_size(_impl_ptr->pcap_ptr, size);
    if (ret != 0)
        SIHD_LOG(error, "Sniffer: cannot set buffer size on an activated capture handle");
    return ret == 0;
}

bool Sniffer::set_datalink(int datalink)
{
    int ret = pcap_set_datalink(_impl_ptr->pcap_ptr, datalink);
    this->_log_if_error(ret);
    return ret == 0;
}

bool Sniffer::set_timestamp_type(int ts_type)
{
    int ret = pcap_set_tstamp_type(_impl_ptr->pcap_ptr, ts_type);
    this->_log_if_error(ret);
    return ret == 0;
}

bool Sniffer::set_timestamp_nano(bool active)
{
    int ret = pcap_set_tstamp_precision(_impl_ptr->pcap_ptr,
                                        active ? PCAP_TSTAMP_PRECISION_NANO : PCAP_TSTAMP_PRECISION_MICRO);
    this->_log_if_error(ret);
    _nano_precision = ret == 0 && active;
    return ret == 0;
}

bool Sniffer::set_snaplen(int len)
{
    int ret = pcap_set_snaplen(_impl_ptr->pcap_ptr, len);
    if (ret != 0)
        SIHD_LOG(error, "Sniffer: cannot set snaplen on an activated capture handle");
    return ret == 0;
}

bool Sniffer::set_timeout(int ms)
{
    int ret = pcap_set_timeout(_impl_ptr->pcap_ptr, ms);
    if (ret != 0)
        SIHD_LOG(error, "Sniffer: cannot set timeout on an activated capture handle");
    return ret == 0;
}

bool Sniffer::set_nonblock(bool block)
{
    char errbuf[PCAP_ERRBUF_SIZE];
    int ret = pcap_setnonblock(_impl_ptr->pcap_ptr, (int)block, errbuf);
    if (ret != 0)
        SIHD_LOG(error, "Sniffer: {}", errbuf);
    return ret == 0;
}

} // namespace sihd::pcap

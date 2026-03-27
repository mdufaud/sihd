#include <pcap.h>

#include <sihd/pcap/PcapReader.hpp>
#include <sihd/pcap/utils.hpp>
#include <sihd/sys/NamedFactory.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

namespace sihd::pcap
{

struct PcapReader::Impl
{
        bool ownership {true};
        pcap_t *pcap_ptr {nullptr};
        u_char *pkt_data_ptr {nullptr};
        struct pcap_pkthdr *pkt_hdr_ptr {nullptr};
        u_int precision {0};
};

SIHD_REGISTER_FACTORY(PcapReader)

SIHD_LOGGER;

PcapReader::PcapReader(const std::string & name, sihd::util::Node *parent):
    sihd::util::Named(name, parent),
    _impl_ptr(std::make_unique<Impl>())
{
    utils::init();
}

PcapReader::~PcapReader()
{
    this->close();
}

bool PcapReader::open(std::string_view path)
{
    this->close();
    char errbuf[PCAP_ERRBUF_SIZE];
    _impl_ptr->pcap_ptr = pcap_open_offline(path.data(), errbuf);
    if (_impl_ptr->pcap_ptr == nullptr)
    {
        SIHD_LOG(error, "PcapReader: {}: {}", errbuf, path);
        return false;
    }
    _impl_ptr->precision = pcap_get_tstamp_precision(_impl_ptr->pcap_ptr);
    return true;
}

bool PcapReader::open_micro_precision(std::string_view path)
{
    return this->_open_precision(path.data(), PCAP_TSTAMP_PRECISION_MICRO);
}

bool PcapReader::open_nano_precision(std::string_view path)
{
    return this->_open_precision(path.data(), PCAP_TSTAMP_PRECISION_NANO);
}

bool PcapReader::_open_precision(const char *path, u_int precision)
{
    this->close();
    char errbuf[PCAP_ERRBUF_SIZE];
    _impl_ptr->pcap_ptr = pcap_open_offline_with_tstamp_precision(path, precision, errbuf);
    if (_impl_ptr->pcap_ptr == nullptr)
    {
        SIHD_LOG(error, "PcapReader: {}: {}", errbuf, path);
        return false;
    }
    _impl_ptr->precision = pcap_get_tstamp_precision(_impl_ptr->pcap_ptr);
    return true;
}

bool PcapReader::open(FILE *file)
{
    this->close();
    char errbuf[PCAP_ERRBUF_SIZE];
    _impl_ptr->pcap_ptr = pcap_fopen_offline(file, errbuf);
    if (_impl_ptr->pcap_ptr == nullptr)
    {
        SIHD_LOG(error, "PcapReader: {}", errbuf);
        return false;
    }
    _impl_ptr->precision = pcap_get_tstamp_precision(_impl_ptr->pcap_ptr);
    return true;
}

bool PcapReader::open_micro_precision(FILE *file)
{
    return this->_open_precision(file, PCAP_TSTAMP_PRECISION_MICRO);
}

bool PcapReader::open_nano_precision(FILE *file)
{
    return this->_open_precision(file, PCAP_TSTAMP_PRECISION_NANO);
}

bool PcapReader::_open_precision(FILE *file, u_int precision)
{
    this->close();
    char errbuf[PCAP_ERRBUF_SIZE];
    _impl_ptr->pcap_ptr = pcap_fopen_offline_with_tstamp_precision(file, precision, errbuf);
    if (_impl_ptr->pcap_ptr == nullptr)
    {
        SIHD_LOG(error, "PcapReader: {}", errbuf);
        return false;
    }
    _impl_ptr->precision = pcap_get_tstamp_precision(_impl_ptr->pcap_ptr);
    return true;
}

bool PcapReader::is_open() const
{
    return _impl_ptr->pcap_ptr != nullptr;
}

bool PcapReader::close()
{
    if (_impl_ptr->pcap_ptr != nullptr)
    {
        if (_impl_ptr->ownership)
            pcap_close(_impl_ptr->pcap_ptr);
        _impl_ptr->pcap_ptr = nullptr;
    }
    _impl_ptr->pkt_data_ptr = nullptr;
    _impl_ptr->pkt_hdr_ptr = nullptr;
    _impl_ptr->ownership = true;
    return true;
}

FILE *PcapReader::file()
{
    return pcap_file(_impl_ptr->pcap_ptr);
}

bool PcapReader::read_next()
{
    int ret = pcap_next_ex(_impl_ptr->pcap_ptr,
                           &_impl_ptr->pkt_hdr_ptr,
                           const_cast<const u_char **>(&_impl_ptr->pkt_data_ptr));
    if (ret == PCAP_ERROR)
        SIHD_LOG(error, "PcapReader: {}", this->error());
    return ret >= 0;
}

bool PcapReader::get_read_data(sihd::util::ArrCharView & view) const
{
    if (_impl_ptr->pkt_data_ptr == nullptr || _impl_ptr->pkt_hdr_ptr == nullptr)
        return false;
    view = {reinterpret_cast<char *>(_impl_ptr->pkt_data_ptr), _impl_ptr->pkt_hdr_ptr->len};
    return true;
}

bool PcapReader::get_read_timestamp(time_t *nano_timestamp) const
{
    if (_impl_ptr->pkt_hdr_ptr == nullptr)
        return false;
    if (_impl_ptr->precision == PCAP_TSTAMP_PRECISION_MICRO)
        *nano_timestamp = sihd::util::time::tv(_impl_ptr->pkt_hdr_ptr->ts);
    else if (_impl_ptr->precision == PCAP_TSTAMP_PRECISION_NANO)
        *nano_timestamp = sihd::util::time::nano_tv(_impl_ptr->pkt_hdr_ptr->ts);
    return true;
}

size_t PcapReader::packet_len() const
{
    if (_impl_ptr->pkt_hdr_ptr == nullptr)
        return 0;
    return _impl_ptr->pkt_hdr_ptr->len;
}

size_t PcapReader::packet_caplen() const
{
    if (_impl_ptr->pkt_hdr_ptr == nullptr)
        return 0;
    return _impl_ptr->pkt_hdr_ptr->caplen;
}

std::string PcapReader::error()
{
    if (_impl_ptr->pcap_ptr == nullptr)
        return {};
    return pcap_geterr(_impl_ptr->pcap_ptr);
}

int PcapReader::snaplen()
{
    return pcap_snapshot(_impl_ptr->pcap_ptr);
}

int PcapReader::datalink()
{
    return pcap_datalink(_impl_ptr->pcap_ptr);
}

int PcapReader::major_version()
{
    return pcap_major_version(_impl_ptr->pcap_ptr);
}

int PcapReader::minor_version()
{
    return pcap_minor_version(_impl_ptr->pcap_ptr);
}

bool PcapReader::is_swapped()
{
    return pcap_is_swapped(_impl_ptr->pcap_ptr) == 1;
}

} // namespace sihd::pcap

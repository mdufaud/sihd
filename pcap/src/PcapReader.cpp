#include <sihd/util/Logger.hpp>
#include <sihd/sys/NamedFactory.hpp>
#include <sihd/util/time.hpp>

#include <sihd/pcap/PcapReader.hpp>

namespace sihd::pcap
{

SIHD_REGISTER_FACTORY(PcapReader)

SIHD_LOGGER;

using namespace sihd::util;

PcapReader::PcapReader(const std::string & name, sihd::util::Node *parent):
    sihd::util::Named(name, parent),
    _ownership(true),
    _pcap_ptr(nullptr),
    _pkt_data_ptr(nullptr),
    _pkt_hdr_ptr(nullptr)
{
    utils::init();
}

PcapReader::~PcapReader()
{
    this->close();
}

void PcapReader::set_pcap(pcap_t *pcap, bool ownership)
{
    this->close();
    _ownership = ownership;
    _pcap_ptr = pcap;
}

bool PcapReader::open(std::string_view path)
{
    this->close();
    char errbuf[PCAP_ERRBUF_SIZE];
    _pcap_ptr = pcap_open_offline(path.data(), errbuf);
    if (_pcap_ptr == nullptr)
    {
        SIHD_LOG(error, "PcapReader: {}: {}", errbuf, path);
        return false;
    }
    _precision = pcap_get_tstamp_precision(_pcap_ptr);
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
    _pcap_ptr = pcap_open_offline_with_tstamp_precision(path, precision, errbuf);
    if (_pcap_ptr == nullptr)
    {
        SIHD_LOG(error, "PcapReader: {}: {}", errbuf, path);
        return false;
    }
    _precision = pcap_get_tstamp_precision(_pcap_ptr);
    return true;
}

bool PcapReader::open(FILE *file)
{
    this->close();
    char errbuf[PCAP_ERRBUF_SIZE];
    _pcap_ptr = pcap_fopen_offline(file, errbuf);
    if (_pcap_ptr == nullptr)
    {
        SIHD_LOG(error, "PcapReader: {}", errbuf);
        return false;
    }
    _precision = pcap_get_tstamp_precision(_pcap_ptr);
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
    _pcap_ptr = pcap_fopen_offline_with_tstamp_precision(file, precision, errbuf);
    if (_pcap_ptr == nullptr)
    {
        SIHD_LOG(error, "PcapReader: {}", errbuf);
        return false;
    }
    _precision = pcap_get_tstamp_precision(_pcap_ptr);
    return true;
}

bool PcapReader::is_open() const
{
    return _pcap_ptr != nullptr;
}

bool PcapReader::close()
{
    if (_pcap_ptr != nullptr)
    {
        if (_ownership)
            pcap_close(_pcap_ptr);
        _pcap_ptr = nullptr;
    }
    _pkt_data_ptr = nullptr;
    _pkt_hdr_ptr = nullptr;
    _ownership = true;
    return true;
}

FILE *PcapReader::file()
{
    return pcap_file(_pcap_ptr);
}

bool PcapReader::read_next()
{
    int ret = pcap_next_ex(_pcap_ptr, &_pkt_hdr_ptr, const_cast<const u_char **>(&_pkt_data_ptr));
    if (ret == PCAP_ERROR)
        SIHD_LOG(error, "PcapReader: {}", this->error());
    return ret >= 0;
}

bool PcapReader::get_read_data(sihd::util::ArrCharView & view) const
{
    if (_pkt_data_ptr == nullptr || _pkt_hdr_ptr == nullptr)
        return false;
    view = {reinterpret_cast<char *>(_pkt_data_ptr), _pkt_hdr_ptr->len};
    return true;
}

bool PcapReader::get_read_timestamp(time_t *nano_timestamp) const
{
    if (_pkt_hdr_ptr == nullptr)
        return false;
    if (_precision == PCAP_TSTAMP_PRECISION_MICRO)
        *nano_timestamp = time::tv(_pkt_hdr_ptr->ts);
    else if (_precision == PCAP_TSTAMP_PRECISION_NANO)
        *nano_timestamp = time::nano_tv(_pkt_hdr_ptr->ts);
    return true;
}

const struct pcap_pkthdr *PcapReader::packet_header() const
{
    return _pkt_hdr_ptr;
}

std::string PcapReader::error()
{
    return pcap_geterr(_pcap_ptr);
}

int PcapReader::snaplen()
{
    return pcap_snapshot(_pcap_ptr);
}

int PcapReader::datalink()
{
    return pcap_datalink(_pcap_ptr);
}

int PcapReader::major_version()
{
    return pcap_major_version(_pcap_ptr);
}

int PcapReader::minor_version()
{
    return pcap_minor_version(_pcap_ptr);
}

bool PcapReader::is_swapped()
{
    return pcap_is_swapped(_pcap_ptr) == 1;
}

} // namespace sihd::pcap

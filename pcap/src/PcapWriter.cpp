#include <sihd/pcap/PcapWriter.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

namespace sihd::pcap
{

SIHD_UTIL_REGISTER_FACTORY(PcapWriter)

LOGGER;

PcapWriter::PcapWriter(const std::string & name, sihd::util::Node *parent):
    sihd::util::Named(name, parent),
    _pcap_ptr(nullptr), _pcap_dumper_ptr(nullptr),
    _clock_ptr(&_default_clock), _linktype(0), _snaplen(65535)
{
    PcapUtils::init();
    this->add_conf("datalink", &PcapWriter::set_datalink);
    this->add_conf("snaplen", &PcapWriter::set_snaplen);
}

PcapWriter::~PcapWriter()
{
    this->close();
}

const char  *PcapWriter::error()
{
    return pcap_geterr(_pcap_ptr);
}

bool    PcapWriter::set_datalink(int dtl)
{
    if (PcapUtils::is_datalink(dtl) == false)
    {
        LOG(error, "PcapWriter: is not a datalink: " << dtl);
        return false;
    }
    _linktype = dtl;
    return true;
}

bool    PcapWriter::set_snaplen(int len)
{
    _snaplen = len;
    return true;
}

bool    PcapWriter::open(const std::string & path, int datalink, int snaplen)
{
    if (this->set_datalink(datalink) && this->set_snaplen(snaplen))
        return this->open(path);
    return false;
}

bool    PcapWriter::open(const std::string & path, int datalink)
{
    if (this->set_datalink(datalink))
        return this->open(path);
    return false;
}

bool    PcapWriter::open(const std::string & path)
{
    this->close();
    _pcap_ptr = pcap_open_dead(_linktype, _snaplen);
    if (_pcap_ptr != nullptr)
    {
        if ((_pcap_dumper_ptr = pcap_dump_open(_pcap_ptr, path.c_str())) == nullptr)
        {
            LOG(error, "PcapWriter: can not open pcap for writing: " << path);
            this->close();
        }
    }
    else
    {
        LOG(error, "PcapWriter: can not open fake pcap for writing");
    }
    return _pcap_dumper_ptr != nullptr;
}

bool    PcapWriter::is_open() const
{
    return _pcap_ptr != nullptr;
}

bool    PcapWriter::close()
{
    if (_pcap_dumper_ptr != nullptr)
    {
        pcap_dump_close(_pcap_dumper_ptr);
        _pcap_dumper_ptr = nullptr;
    }
    if (_pcap_ptr != nullptr)
    {
        pcap_close(_pcap_ptr);
        _pcap_ptr = nullptr;
    }
    return true;
}

FILE    *PcapWriter::file()
{
    return pcap_dump_file(_pcap_dumper_ptr);
}

bool    PcapWriter::write(const char *data, size_t size)
{
    pcap_pkthdr hdr;
    hdr.caplen = size;
    hdr.len = size;
    hdr.ts = sihd::util::time::to_tv(_clock_ptr->now());
    pcap_dump((u_char *)_pcap_dumper_ptr, &hdr, (const u_char *)data);
    return true;
}

bool    PcapWriter::write(const char *data, size_t size, time_t nano)
{
    pcap_pkthdr hdr;
    hdr.caplen = size;
    hdr.len = size;
    hdr.ts = sihd::util::time::to_tv(nano);
    pcap_dump((u_char *)_pcap_dumper_ptr, &hdr, (const u_char *)data);
    return true;
}

bool    PcapWriter::write(const char *data, size_t size, time_t sec, time_t usec)
{
    pcap_pkthdr hdr;
    hdr.caplen = size;
    hdr.len = size;
    hdr.ts.tv_sec = sec;
    hdr.ts.tv_usec = usec;
    pcap_dump((u_char *)_pcap_dumper_ptr, &hdr, (const u_char *)data);
    return true;
}

int64_t PcapWriter::pos()
{
    return pcap_dump_ftell64(_pcap_dumper_ptr);
}

bool    PcapWriter::flush()
{
    return pcap_dump_flush(_pcap_dumper_ptr) == 0;
}

int     PcapWriter::snaplen()
{
    return pcap_snapshot(_pcap_ptr);
}

int     PcapWriter::datalink()
{
    return pcap_datalink(_pcap_ptr);
}

}
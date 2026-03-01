#ifndef __SIHD_PCAP_PCAPREADER_HPP__
#define __SIHD_PCAP_PCAPREADER_HPP__

#include <sihd/sys/File.hpp>
#include <sihd/util/IReader.hpp>
#include <sihd/util/Node.hpp>

#pragma message("TODO pImpl")
#include <sihd/pcap/utils.hpp>

namespace sihd::pcap
{

class PcapReader: public sihd::util::Named,
                  public sihd::util::IReaderTimestamp
{
    public:
        PcapReader(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~PcapReader();

        bool open(std::string_view path);
        bool open_micro_precision(std::string_view path);
        bool open_nano_precision(std::string_view path);
        bool open(FILE *file);
        bool open_micro_precision(FILE *file);
        bool open_nano_precision(FILE *file);
        bool close();
        bool is_open() const;
        void set_pcap(pcap_t *pcap, bool ownership = false);

        bool is_swapped();

        // should check if file is_open before calling theses

        bool read_next();
        bool get_read_data(sihd::util::ArrCharView & view) const;
        bool get_read_timestamp(time_t *nano_timestamp) const;

        const struct pcap_pkthdr *packet_header() const;

        FILE *file();
        int major_version();
        int minor_version();
        int snaplen();
        int datalink();

        std::string error();

        pcap_t *pcap() { return _pcap_ptr; }

    protected:

    private:
        bool _open_precision(const char *path, u_int precision);
        bool _open_precision(FILE *file, u_int precision);

        bool _ownership;
        pcap_t *_pcap_ptr;
        u_char *_pkt_data_ptr;
        struct pcap_pkthdr *_pkt_hdr_ptr;
        u_int _precision;
};

} // namespace sihd::pcap

#endif
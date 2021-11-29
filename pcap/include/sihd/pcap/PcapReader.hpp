#ifndef __SIHD_PCAP_PCAPREADER_HPP__
# define __SIHD_PCAP_PCAPREADER_HPP__

# include <sihd/util/Node.hpp>
# include <sihd/util/File.hpp>
# include <sihd/pcap/PcapUtils.hpp>

namespace sihd::pcap
{

class PcapReader:   public sihd::util::Named
{
    public:
        PcapReader(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~PcapReader();

        bool open(const std::string & path);
        bool open_micro_precision(const std::string & path);
        bool open_nano_precision(const std::string & path);
        bool open(FILE *file);
        bool open_micro_precision(FILE *file);
        bool open_nano_precision(FILE *file);
        bool close();
        bool is_open() const;
        void set_pcap(pcap_t *pcap, bool ownership = false);

        bool is_swapped();

        // should check if file is_open before calling theses

        bool read_next();
        bool get_data(char **data, size_t *size) const;
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
};

}

#endif
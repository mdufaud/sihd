#ifndef __SIHD_PCAP_PCAPREADER_HPP__
#define __SIHD_PCAP_PCAPREADER_HPP__

#include <memory>

#include <sihd/sys/File.hpp>
#include <sihd/util/IReader.hpp>
#include <sihd/util/Node.hpp>

namespace sihd::pcap
{

class PcapReader: public sihd::util::Named,
                  public sihd::util::IReaderTimestamp
{
    public:
        PcapReader(const std::string & name, sihd::util::Node *parent = nullptr);
        ~PcapReader();

        bool open(std::string_view path);
        bool open_micro_precision(std::string_view path);
        bool open_nano_precision(std::string_view path);
        bool open(FILE *file);
        bool open_micro_precision(FILE *file);
        bool open_nano_precision(FILE *file);
        bool close();
        bool is_open() const;

        bool is_swapped();

        // should check if file is_open before calling theses

        bool read_next();
        bool get_read_data(sihd::util::ArrCharView & view) const;
        bool get_read_timestamp(time_t *nano_timestamp) const;

        size_t packet_len() const;
        size_t packet_caplen() const;

        FILE *file();
        int major_version();
        int minor_version();
        int snaplen();
        int datalink();

        std::string error();

    private:
        bool _open_precision(const char *path, unsigned int precision);
        bool _open_precision(FILE *file, unsigned int precision);

        struct Impl;
        std::unique_ptr<Impl> _impl_ptr;
};

} // namespace sihd::pcap

#endif
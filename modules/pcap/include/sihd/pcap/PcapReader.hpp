#ifndef __SIHD_PCAP_PCAPREADER_HPP__
#define __SIHD_PCAP_PCAPREADER_HPP__

#include <memory>

#include <sihd/sys/File.hpp>
#include <sihd/util/IReader.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/ZStringView.hpp>

namespace sihd::pcap
{

class PcapReader: public sihd::util::Named,
                  public sihd::util::IReaderTimestamp
{
    public:
        PcapReader(const std::string & name, sihd::util::Node *parent = nullptr);
        ~PcapReader();

        sihd::util::Result<void> open(sihd::util::ZStringView path);
        sihd::util::Result<void> open_micro_precision(sihd::util::ZStringView path);
        sihd::util::Result<void> open_nano_precision(sihd::util::ZStringView path);
        sihd::util::Result<void> open(FILE *file);
        sihd::util::Result<void> open_micro_precision(FILE *file);
        sihd::util::Result<void> open_nano_precision(FILE *file);
        bool close();
        bool is_open() const;

        bool is_swapped();

        // should check if file is_open before calling theses

        // false means end of read, an error means the read failed
        sihd::util::Result<bool> read_next();
        bool get_read_data(sihd::util::ArrCharView & view) const;
        sihd::util::Result<sihd::util::Timestamp> get_read_timestamp() const;

        size_t packet_len() const;
        size_t packet_caplen() const;

        FILE *file();
        int major_version();
        int minor_version();
        int snaplen();
        int datalink();

        std::string error();

    private:
        sihd::util::Result<void> _open_precision(const char *path, unsigned int precision);
        sihd::util::Result<void> _open_precision(FILE *file, unsigned int precision);

        struct Impl;
        std::unique_ptr<Impl> _impl_ptr;
};

} // namespace sihd::pcap

#endif
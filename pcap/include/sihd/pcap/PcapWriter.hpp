#ifndef __SIHD_PCAP_PCAPWRITER_HPP__
#define __SIHD_PCAP_PCAPWRITER_HPP__

#include <sihd/util/Clocks.hpp>
#include <sihd/util/Configurable.hpp>
#include <sihd/util/IWriter.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/time.hpp>

#include <sihd/pcap/utils.hpp>

namespace sihd::pcap
{

class PcapWriter: public sihd::util::Named,
                  public sihd::util::Configurable,
                  public sihd::util::IWriterTimestamp
{
    public:
        PcapWriter(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~PcapWriter();

        bool open(std::string_view path);
        bool open(std::string_view path, int datalink);
        bool open(std::string_view path, int datalink, int snaplen);
        bool close();
        bool is_open() const;

        bool set_snaplen(int len);
        bool set_datalink(int dtl);

        ssize_t write(sihd::util::ArrCharView view);
        ssize_t write(sihd::util::ArrCharView view, sihd::util::Timestamp timestamp);
        ssize_t write(sihd::util::ArrCharView view, time_t sec, time_t usec);

        FILE *file();
        int64_t pos();
        bool flush();
        int snaplen();
        int datalink();

        const char *error();

        pcap_t *pcap() { return _pcap_ptr; }
        pcap_dumper_t *pcap_dumper() { return _pcap_dumper_ptr; }

    protected:

    private:
        pcap_t *_pcap_ptr;
        pcap_dumper_t *_pcap_dumper_ptr;
        sihd::util::IClock *_clock_ptr;
        sihd::util::SystemClock _default_clock;
        int _linktype;
        int _snaplen;
};

} // namespace sihd::pcap

#endif
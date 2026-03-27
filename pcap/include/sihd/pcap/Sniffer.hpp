#ifndef __SIHD_PCAP_SNIFFER_HPP__
#define __SIHD_PCAP_SNIFFER_HPP__

#include <cstdint>
#include <memory>
#include <optional>

#include <sihd/util/ABlockingService.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/Configurable.hpp>
#include <sihd/util/IReader.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/Observable.hpp>

namespace sihd::pcap
{

class Sniffer: public sihd::util::Named,
               public sihd::util::Configurable,
               public sihd::util::IReaderTimestamp,
               public sihd::util::ABlockingService,
               public sihd::util::Observable<Sniffer>
{
    public:
        struct Stats
        {
                uint32_t received;
                uint32_t dropped;
                uint32_t if_dropped;
        };

        Sniffer(const std::string & name, sihd::util::Node *parent = nullptr);
        ~Sniffer();

        bool open(const std::string & source);
        bool close();
        bool is_open() const;

        bool activate();
        bool is_active() const;

        // sniff bufferful of packets from a pcap_t open for a live capture then leave
        bool sniff();
        // sniff one packet
        bool read_next() override;
        // get datas
        bool get_read_data(sihd::util::ArrCharView & view) const override;
        bool get_read_timestamp(time_t *nano_timestamp) const override;
        const sihd::util::ArrByte & data() const;

        // maximum pkts to sniff before stopping
        bool set_max_sniff(size_t n);
        // protocol linux
        bool set_linux_protocol(int protocol);
        // promiscuous mode
        bool set_promiscuous(bool active);
        // rfmon monitor mode
        bool set_monitor(bool active);
        // no buffering
        bool set_immediate(bool active);
        // micro or nano precision
        bool set_timestamp_nano(bool active);
        // non blocking sniff
        bool set_nonblock(bool block);
        bool set_datalink(int datalink);
        bool set_timestamp_type(int ts_type);
        bool set_buffer_size(int size);
        bool set_snaplen(int len);
        bool set_timeout(int ms);
        // in / out / both
        bool set_direction(std::string_view direction);

        bool set_filter(std::string_view filter);

        std::vector<int> datalinks();
        std::vector<int> timestamp_types();

        bool can_monitor();
        bool is_nonblock();
        bool timestamp_nano();
        bool timestamp_micro();
        int timestamp_type();
        int snaplen();
        int datalink();

        int pollable_fd();
        std::optional<Stats> stats();

        std::string error();

        const sihd::util::ArrByte & array() const { return _array; }

    protected:
        bool on_start() override;
        bool on_stop() override;

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl_ptr;

        void _new_packet(const void *hdr, const void *bytes);
        void _log_if_error(int ret);

        bool _active;
        int _max_sniff;
        int _timestamp_type;
        bool _nano_precision;

        time_t _pkt_nano_timestamp;
        sihd::util::ArrByte _array;
};

} // namespace sihd::pcap

#endif
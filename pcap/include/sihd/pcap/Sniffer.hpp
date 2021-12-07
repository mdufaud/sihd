#ifndef __SIHD_PCAP_SNIFFER_HPP__
# define __SIHD_PCAP_SNIFFER_HPP__

# include <sihd/util/Node.hpp>
# include <sihd/util/Configurable.hpp>
# include <sihd/util/IStoppableRunnable.hpp>
# include <sihd/util/Observable.hpp>
# include <sihd/util/Array.hpp>
# include <sihd/pcap/PcapUtils.hpp>

namespace sihd::pcap
{

class Sniffer:  public sihd::util::Named,
                public sihd::util::Configurable,
                public sihd::util::IStoppableRunnable,
                public sihd::util::Observable<Sniffer>
{
    public:
        Sniffer(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~Sniffer();

        bool open(const std::string & source);
        bool close();
        bool is_open() const;

        bool activate();
        bool is_active() const;

        bool run();
        bool is_running() const;
        bool stop();

        // sniff bufferful of packets from a pcap_t open for a live capture then leave
        bool sniff();
        bool sniff_one();
        void new_packet(const struct pcap_pkthdr *h, const u_char *bytes);
        // get datas
        time_t timestamp() const;
        const sihd::util::ArrByte & data() const;
        bool get_data(char **data, size_t *size) const;

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
        bool set_buffersize(int size);
        bool set_snaplen(int len);
        bool set_timeout(int ms);
        // in / out / both
        bool set_direction(const std::string & direction);

        bool set_filter(const std::string & filter);

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
        const struct timeval *poll_timeout();
        const struct pcap_stat *stats();

        std::string error();
        
        pcap_t *pcap() { return _pcap_ptr; }
        const sihd::util::ArrByte & array() const { return _array; }

    protected:
        static void _callback(u_char *user, const struct pcap_pkthdr *h, const u_char *bytes);

    private:
        void _log_if_error(int ret);

        bool _active;
        bool _running;
        int _max_sniff;
        int _timestamp_type;
        bool _nano_precision;
        
        pcap_t *_pcap_ptr;
        struct pcap_stat _pcap_stats;

        time_t _pkt_nano_timestamp;
        sihd::util::ArrByte _array;
};

}

#endif
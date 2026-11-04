#ifndef __SIHD_PCAP_DEVCAPTURE_HPP__
#define __SIHD_PCAP_DEVCAPTURE_HPP__

#include <cstdint>
#include <optional>
#include <string>

#include <sihd/core/Device.hpp>
#include <sihd/pcap/PcapReader.hpp>
#include <sihd/pcap/Sniffer.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/Waitable.hpp>
#include <sihd/util/Worker.hpp>

namespace sihd::pcap
{

class DevCapture: public sihd::core::Device
{
    public:
        enum class Status : uint8_t
        {
            idle,
            running,
            paused,
            end_of_file,
            failed,
        };

        // packets longer than libpcap's maximum snaplen are cut by libpcap
        static constexpr size_t max_packet_size = 262144;

        DevCapture(const std::string & name, sihd::util::Node *parent = nullptr);
        ~DevCapture() override;

        bool is_running() const override;

        bool set_interface(const std::string & interface_name);
        bool set_file(const std::string & path);
        bool set_realtime(bool active);
        bool set_promiscuous(bool active);
        bool set_monitor(bool active);
        bool set_immediate(bool active);
        bool set_timeout(int ms);
        bool set_stats_period(int ms);

    protected:
        using sihd::core::Device::handle;

        void handle(sihd::core::Channel *channel) override;

        bool on_init() override;
        bool on_start() override;
        bool on_stop() override;

    private:
        bool _open_source();
        bool _capture();
        sihd::util::IReaderTimestamp & _source();
        bool _wait_capture_pace(sihd::util::Timestamp timestamp);
        void _publish_packet();
        void _publish_kernel_stats();
        void _set_status(Status status);

        std::string _interface;
        std::string _file;
        bool _realtime;
        bool _promiscuous;
        bool _monitor;
        bool _immediate;
        int _timeout_ms;
        sihd::util::Duration _stats_period;

        std::optional<Sniffer> _sniffer;
        std::optional<PcapReader> _reader;
        int _datalink;

        sihd::util::Worker _worker;
        sihd::util::Waitable _waitable;
        bool _active;
        bool _stopping;

        std::optional<sihd::util::Timestamp> _last_capture_time;
        sihd::util::Timestamp _last_stats_time;

        sihd::core::Channel *_activate;
        sihd::core::Channel *_packet;
        sihd::core::Channel *_datalink_out;
        sihd::core::Channel *_status;
        sihd::core::Channel *_kernel_received;
        sihd::core::Channel *_kernel_dropped;
};

} // namespace sihd::pcap

#endif

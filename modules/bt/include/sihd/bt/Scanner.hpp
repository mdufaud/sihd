#ifndef __SIHD_BT_SCANNER_HPP__
#define __SIHD_BT_SCANNER_HPP__

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <sihd/bt/adapter.hpp>
#include <sihd/util/ABlockingService.hpp>
#include <sihd/util/Clocks.hpp>
#include <sihd/util/Configurable.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/Named.hpp>
#include <sihd/util/Observable.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/Waitable.hpp>

namespace sihd::bt
{

enum class Transport
{
    any,
    le,
    classic
};

// what a scan reports: every field left empty lets everything through
struct ScanFilter
{
        Transport transport = Transport::any;
        std::vector<std::string> uuids;
        std::optional<int16_t> min_rssi;
};

struct ScanResult
{
        std::string identifier;
        std::string address;
        AddressType address_type = AddressType::unspecified;
        int16_t rssi = 0;
        std::optional<int16_t> tx_power;
        // false where the stack does not report it
        bool connectable = false;
        std::map<uint16_t, std::vector<uint8_t>> manufacturer_data;
        // keyed by service uuid
        std::map<std::string, std::vector<uint8_t>> service_data;
        std::vector<std::string> uuids;
        std::optional<uint16_t> appearance;
        std::optional<uint32_t> class_of_device;
        sihd::util::Timestamp timestamp;
};

// device discovery: start() blocks until stop(), observers are notified on each new, changed or lost device
class Scanner: public sihd::util::Named,
               public sihd::util::Configurable,
               public sihd::util::ABlockingService,
               public sihd::util::Observable<Scanner>
{
    public:
        enum class Event
        {
            found,
            updated,
            // results() drops it once the observers returned
            lost
        };

        Scanner(const std::string & name, sihd::util::Node *parent = nullptr);
        ~Scanner();

        bool set_adapter(size_t index);
        // before start()
        bool set_filter(const ScanFilter & filter);
        // before start(): a device silent that long is lost, 0 never loses one
        bool set_lost_timeout(sihd::util::Duration timeout);

        bool scan_for(sihd::util::Duration duration);

        // read them from an observer or once the scan stopped
        const std::map<std::string, ScanResult> & results() const;
        std::optional<ScanResult> last_result() const;
        Event last_event() const;

    protected:
        bool on_start() override;
        bool on_stop() override;

    private:
        bool _scan();
        void _publish(ScanResult && result);
        void _forget_lost();
        std::optional<sihd::util::Duration> _next_wait(sihd::util::Duration period) const;

        sihd::util::Waitable _waitable;
        // set by a platform loop blocked outside _waitable, guarded by it
        std::function<void()> _wake_scan;
        size_t _adapter_index;
        ScanFilter _filter;
        sihd::util::Duration _lost_timeout;
        std::optional<sihd::util::Timestamp> _deadline;
        sihd::util::SteadyClock _clock;
        std::map<std::string, ScanResult> _results;
        // when _clock last heard each device, an unchanged advertisement too
        std::map<std::string, sihd::util::Timestamp> _heard;
        std::string _last_address;
        Event _last_event = Event::found;
};

} // namespace sihd::bt

#endif

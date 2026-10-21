#ifndef __SIHD_BT_SCANNER_HPP__
#define __SIHD_BT_SCANNER_HPP__

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

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

enum class AddressType
{
    public_address,
    random,
    unspecified
};

struct ScanResult
{
        std::string identifier;
        std::string address;
        AddressType address_type = AddressType::unspecified;
        int16_t rssi = 0;
        std::optional<int16_t> tx_power;
        // bluez does not expose it: always false on linux
        bool connectable = false;
        std::map<uint16_t, std::vector<uint8_t>> manufacturer_data;
        // keyed by service uuid
        std::map<std::string, std::vector<uint8_t>> service_data;
        std::vector<std::string> uuids;
        sihd::util::Timestamp timestamp;
};

struct AdapterInfo
{
        std::string identifier;
        std::string address;
};

sihd::util::Result<bool> bluetooth_enabled();
sihd::util::Result<std::vector<AdapterInfo>> list_adapters();

// BLE discovery: start() blocks until stop(), observers are notified on each new or changed device
class Scanner: public sihd::util::Named,
               public sihd::util::Configurable,
               public sihd::util::ABlockingService,
               public sihd::util::Observable<Scanner>
{
    public:
        Scanner(const std::string & name, sihd::util::Node *parent = nullptr);
        ~Scanner();

        bool set_adapter(size_t index);

        bool scan_for(sihd::util::Duration duration);

        // read them from an observer or once the scan stopped
        const std::map<std::string, ScanResult> & results() const;
        std::optional<ScanResult> last_result() const;

    protected:
        bool on_start() override;
        bool on_stop() override;

    private:
        bool _scan();
        void _publish(ScanResult && result);
        std::optional<sihd::util::Duration> _next_wait(sihd::util::Duration period) const;

        sihd::util::Waitable _waitable;
        std::atomic<bool> _stop_requested;
        // set by a platform loop blocked outside _waitable, guarded by it
        std::function<void()> _wake_scan;
        size_t _adapter_index;
        std::optional<sihd::util::Timestamp> _deadline;
        sihd::util::SteadyClock _clock;
        std::map<std::string, ScanResult> _results;
        std::string _last_address;
};

} // namespace sihd::bt

#endif

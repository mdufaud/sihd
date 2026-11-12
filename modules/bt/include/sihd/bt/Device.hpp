#ifndef __SIHD_BT_DEVICE_HPP__
#define __SIHD_BT_DEVICE_HPP__

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <sihd/bt/Gatt.hpp>
#include <sihd/bt/adapter.hpp>
#include <sihd/util/ABlockingService.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Configurable.hpp>
#include <sihd/util/Named.hpp>
#include <sihd/util/Observable.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::bt
{

struct DeviceInfo
{
        std::string address;
        AddressType address_type = AddressType::unspecified;
        std::string name;
        bool paired = false;
        bool connected = false;
        bool trusted = false;
        std::optional<uint16_t> appearance;
        std::optional<uint32_t> class_of_device;
        std::vector<std::string> uuids;
};

struct PairingRequest
{
        enum class Kind
        {
            confirm,
            confirm_passkey,
            display_passkey,
            enter_passkey,
            display_pin,
            enter_pin
        };

        std::string address;
        Kind kind = Kind::confirm;
        uint32_t passkey = 0;
        std::string pin;
};

struct PairingAnswer
{
        bool accept = false;
        uint32_t passkey = 0;
        std::string pin;
};

using PairingHandler = std::function<PairingAnswer(const PairingRequest &)>;

// a remote device: start() connects and blocks until stop() or a lost link, observers run on that thread
class Device: public sihd::util::Named,
              public sihd::util::Configurable,
              public sihd::util::ABlockingService,
              public sihd::util::Observable<Device>
{
    public:
        enum class Event
        {
            connected,
            disconnected,
            notification
        };

        Device(const std::string & name, sihd::util::Node *parent = nullptr);
        ~Device();

        bool set_address(const std::string & address);
        bool set_adapter(size_t index);
        // without one the pairing goes through the system agent
        void set_pairing_handler(PairingHandler handler);

        sihd::util::Result<DeviceInfo> info() const;
        sihd::util::Result<void> pair();
        sihd::util::Result<void> cancel_pairing();
        sihd::util::Result<void> unpair();
        sihd::util::Result<void> set_trusted(bool trusted);

        // these need the device connected
        sihd::util::Result<std::vector<GattService>> services();
        sihd::util::Result<std::vector<uint8_t>> read_characteristic(const GattCharacteristic & characteristic);
        sihd::util::Result<void> write_characteristic(const GattCharacteristic & characteristic,
                                                      sihd::util::ArrUByteView value,
                                                      WriteType type);
        sihd::util::Result<std::vector<uint8_t>> read_descriptor(const GattDescriptor & descriptor);
        sihd::util::Result<void> write_descriptor(const GattDescriptor & descriptor, sihd::util::ArrUByteView value);
        sihd::util::Result<void> subscribe(const GattCharacteristic & characteristic);
        sihd::util::Result<void> unsubscribe(const GattCharacteristic & characteristic);
        sihd::util::Result<uint16_t> mtu();

        // read them from an observer
        Event last_event() const;
        std::optional<GattNotification> last_notification() const;

    protected:
        bool on_start() override;
        bool on_stop() override;

    private:
        struct Impl;

        void _add_confs();
        void _publish_event(Event event);
        // from any thread, the start() thread publishes it
        void _queue_notification(GattNotification && notification);
        void _flush_notifications();
        void _wake_loop();

        std::unique_ptr<Impl> _impl;
        std::string _address;
        size_t _adapter_index;
        PairingHandler _pairing_handler;
        Event _last_event;
        std::optional<GattNotification> _last_notification;
        std::mutex _pending_mutex;
        std::deque<GattNotification> _pending;
};

} // namespace sihd::bt

#endif

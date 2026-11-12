#ifndef __SIHD_BT_GATTSERVER_HPP__
#define __SIHD_BT_GATTSERVER_HPP__

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <sihd/bt/Gatt.hpp>
#include <sihd/util/ABlockingService.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Configurable.hpp>
#include <sihd/util/Named.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::bt
{

struct LocalCharacteristic
{
        std::string uuid;
        // GattCharacteristic::Property bits
        uint8_t properties = 0;
        // run from a backend thread, a read without handler answers the last notified or written value
        std::function<std::vector<uint8_t>()> on_read;
        // false answers the remote device that the write is not permitted
        std::function<bool(const std::vector<uint8_t> &)> on_write;
};

struct LocalService
{
        std::string uuid;
        bool primary = true;
        std::vector<LocalCharacteristic> characteristics;
};

// the services remote devices read: start() publishes them and blocks until stop()
class GattServer: public sihd::util::Named,
                  public sihd::util::Configurable,
                  public sihd::util::ABlockingService
{
    public:
        GattServer(const std::string & name, sihd::util::Node *parent = nullptr);
        ~GattServer();

        bool set_adapter(size_t index);
        // before start()
        bool add_service(const LocalService & service);

        // to the remote devices that subscribed, while started
        sihd::util::Result<void> notify(const std::string & service_uuid,
                                        const std::string & characteristic_uuid,
                                        sihd::util::ArrUByteView value);

    protected:
        bool on_start() override;
        bool on_stop() override;

    private:
        struct Impl;

        std::unique_ptr<Impl> _impl;
        size_t _adapter_index;
        std::vector<LocalService> _services;
};

} // namespace sihd::bt

#endif

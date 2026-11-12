#ifndef __SIHD_BT_GATT_HPP__
#define __SIHD_BT_GATT_HPP__

#include <cstdint>
#include <string>
#include <vector>

#include <sihd/util/Timestamp.hpp>

namespace sihd::bt
{

enum class WriteType
{
    with_response,
    without_response
};

struct GattDescriptor
{
        std::string uuid;
        uint16_t handle = 0;
};

struct GattCharacteristic
{
        // the characteristic properties bits of the Bluetooth core specification
        enum Property : uint8_t
        {
            broadcast = 0x01,
            read = 0x02,
            write_without_response = 0x04,
            write = 0x08,
            notify = 0x10,
            indicate = 0x20,
            signed_write = 0x40,
            extended_properties = 0x80,
        };

        std::string uuid;
        uint16_t handle = 0;
        uint8_t properties = 0;
        std::vector<GattDescriptor> descriptors;
};

struct GattService
{
        std::string uuid;
        uint16_t handle = 0;
        bool primary = true;
        std::vector<GattCharacteristic> characteristics;
};

struct GattNotification
{
        uint16_t handle = 0;
        std::string uuid;
        std::vector<uint8_t> value;
        sihd::util::Timestamp timestamp;
};

} // namespace sihd::bt

#endif

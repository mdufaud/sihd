#ifndef __SIHD_BT_BTUTILS_HPP__
#define __SIHD_BT_BTUTILS_HPP__

#include <string>
#include <vector>

namespace sihd::bt
{

struct BtDevice
{
        std::string identifier;
        std::string address;
        int16_t rssi;
        bool connectable;
};

class BtUtils
{
    public:
        BtUtils();
        virtual ~BtUtils();

        // Check if bluetooth is available on the system
        static bool bluetooth_enabled();

        // Scan for BLE devices for the given duration (ms)
        static std::vector<BtDevice> scan(int timeout_ms = 5000);

    protected:

    private:
};

} // namespace sihd::bt

#endif
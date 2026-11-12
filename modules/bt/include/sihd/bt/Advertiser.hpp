#ifndef __SIHD_BT_ADVERTISER_HPP__
#define __SIHD_BT_ADVERTISER_HPP__

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <sihd/util/ABlockingService.hpp>
#include <sihd/util/Configurable.hpp>
#include <sihd/util/Named.hpp>

namespace sihd::bt
{

struct Advertisement
{
        std::string local_name;
        std::vector<std::string> service_uuids;
        std::map<uint16_t, std::vector<uint8_t>> manufacturer_data;
        // keyed by service uuid
        std::map<std::string, std::vector<uint8_t>> service_data;
        bool connectable = true;
};

// BLE advertising: start() advertises and blocks until stop()
class Advertiser: public sihd::util::Named,
                  public sihd::util::Configurable,
                  public sihd::util::ABlockingService
{
    public:
        Advertiser(const std::string & name, sihd::util::Node *parent = nullptr);
        ~Advertiser();

        bool set_adapter(size_t index);
        bool set_advertisement(const Advertisement & advertisement);

    protected:
        bool on_start() override;
        bool on_stop() override;

    private:
        struct Impl;

        std::unique_ptr<Impl> _impl;
        size_t _adapter_index;
        Advertisement _advertisement;
};

} // namespace sihd::bt

#endif

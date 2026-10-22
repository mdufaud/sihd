#include <thread>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <sihd/bt/Scanner.hpp>
#include <sihd/sys/App.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

using namespace sihd::util;
using namespace sihd::bt;

SIHD_NEW_LOGGER("demo");

namespace demo
{

std::string address_type_str(AddressType type)
{
    switch (type)
    {
        case AddressType::public_address:
            return "public";
        case AddressType::random:
            return "random";
        default:
            return "unspecified";
    }
}

std::string short_uuid(const std::string & uuid)
{
    constexpr std::string_view base_suffix = "-0000-1000-8000-00805f9b34fb";
    if (uuid.size() == 36 && uuid.starts_with("0000") && uuid.ends_with(base_suffix))
        return fmt::format("0x{}", uuid.substr(4, 4));
    return uuid;
}

std::string describe(const ScanResult & result)
{
    std::vector<std::string> manufacturers;
    for (const auto & [company_id, data] : result.manufacturer_data)
        manufacturers.emplace_back(fmt::format("0x{:04x}:{:02x}", company_id, fmt::join(data, "")));
    std::vector<std::string> services;
    for (const auto & [uuid, data] : result.service_data)
        services.emplace_back(fmt::format("{}:{:02x}", short_uuid(uuid), fmt::join(data, "")));
    std::vector<std::string> uuids;
    for (const std::string & uuid : result.uuids)
        uuids.emplace_back(short_uuid(uuid));

    return fmt::format("{} ({}) {:>4} dBm tx={} name='{}' connectable={} uuids=[{}] manufacturers=[{}] services=[{}]",
                       result.address,
                       address_type_str(result.address_type),
                       result.rssi,
                       result.tx_power.has_value() ? fmt::format("{}", *result.tx_power) : "-",
                       result.identifier.empty() ? "-" : result.identifier,
                       result.connectable,
                       fmt::join(uuids, ", "),
                       fmt::join(manufacturers, ", "),
                       fmt::join(services, ", "));
}

void list_bluetooth()
{
    auto enabled = bluetooth_enabled();
    if (SIHD_UNEXPECTED_LOG(enabled))
        return;
    SIHD_LOG(info, "bluetooth enabled: {}", *enabled);

    auto adapters = list_adapters();
    if (SIHD_UNEXPECTED_LOG(adapters))
        return;
    for (size_t index = 0; index < adapters->size(); ++index)
        SIHD_LOG(info, "adapter {}: {} [{}]", index, (*adapters)[index].identifier, (*adapters)[index].address);
}

} // namespace demo

int main(int argc, char **argv)
{
    sihd::sys::App app({
        .name = "bt_scan_demo",
        .description = "BLE scan demo of module bt",
    });

    int duration = 10;
    int adapter = 0;
    app.root().bind("duration", duration, "Scan duration in seconds, 0 scans until ctrl+C", "d");
    app.root().bind("adapter", adapter, "Adapter index", "a");

    app.root().on_run([&] {
        demo::list_bluetooth();

        Scanner scanner("scanner");
        Handler<Scanner *> printer([](Scanner *obj) {
            if (auto result = obj->last_result(); result.has_value())
                SIHD_LOG(info, "{}", demo::describe(*result));
        });
        scanner.add_observer(&printer);
        SIHD_DIE_FALSE(scanner.set_adapter(static_cast<size_t>(adapter)));

        SIHD_LOG(notice, "Scanning {}, press ctrl+C to stop", duration > 0 ? fmt::format("for {}s", duration) : "");
        bool scanned = false;
        std::jthread scan_thread([&] {
            scanned = duration > 0 ? scanner.scan_for(Duration(time::sec(duration))) : scanner.start();
            app.stop();
        });
        app.loop();
        if (scanner.is_running())
            scanner.stop();
        scan_thread.join();

        if (!scanned)
        {
            SIHD_LOG(error, "Scan failed");
            app.exit(EXIT_FAILURE);
            return;
        }
        SIHD_COUT("\n{} devices:\n", scanner.results().size());
        for (const auto & [address, result] : scanner.results())
            SIHD_COUT("  {}\n", demo::describe(result));
    });

    return app.run(argc, argv);
}

#include <cstdlib>
#include <thread>

#include <gtest/gtest.h>

#include <sihd/bt/Scanner.hpp>
#include <sihd/sys/env.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>
#include <sihd/util/time.hpp>

namespace test
{
SIHD_NEW_LOGGER("test");
using namespace sihd::bt;
using namespace sihd::util;
using namespace sihd::sys;

class TestBtScan: public ::testing::Test
{
    protected:
        TestBtScan() { LoggerManager::stream(); }

        virtual ~TestBtScan() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}

        static bool has_powered_adapter() { return bluetooth_enabled().value_or(false); }
};

TEST_F(TestBtScan, test_no_system_bus)
{
    if constexpr (!build::is_linux)
        GTEST_SKIP() << "the dbus system bus is linux only";
    // libdbus reads the bus address once per process: only a fresh process sees the unreachable one
    GTEST_FLAG_SET(death_test_style, "threadsafe");

    EXPECT_EXIT(
        {
            (void)env::set("DBUS_SYSTEM_BUS_ADDRESS", "unix:path=/nonexistent/sihd_bt_test_socket");
            Scanner scanner("scanner");
            const bool unreachable = !bluetooth_enabled().has_value() && !list_adapters().has_value()
                                     && !scanner.scan_for(Duration(time::milli(100))) && scanner.results().empty();
            std::exit(unreachable ? EXIT_SUCCESS : EXIT_FAILURE);
        },
        testing::ExitedWithCode(EXIT_SUCCESS),
        "");
}

TEST_F(TestBtScan, test_list_adapters)
{
    auto adapters = list_adapters();
    if (!adapters.has_value())
    {
        GTEST_SKIP() << "no bluetooth stack: " << adapters.error().message;
    }

    for (const AdapterInfo & adapter : *adapters)
    {
        SIHD_LOG(info, "Adapter: {} [{}]", adapter.identifier, adapter.address);
        EXPECT_FALSE(adapter.identifier.empty());
        EXPECT_EQ(adapter.address.size(), 17u);
    }
}

TEST_F(TestBtScan, test_scan_for)
{
    if (!has_powered_adapter())
        GTEST_SKIP() << "no powered bluetooth adapter";
    Scanner scanner("scanner");
    size_t notifications = 0;
    Handler<Scanner *> counter([&notifications](Scanner *) { ++notifications; });
    scanner.add_observer(&counter);

    const bool scanned = scanner.scan_for(Duration(time::sec(3)));

    EXPECT_TRUE(scanned);
    EXPECT_GE(notifications, scanner.results().size());
    for (const auto & [address, result] : scanner.results())
    {
        SIHD_LOG(info, "Device: '{}' [{}] {} dBm", result.identifier, address, result.rssi);
        EXPECT_EQ(result.address, address);
        EXPECT_EQ(address.size(), 17u);
        for (const auto & [uuid, data] : result.service_data)
            EXPECT_EQ(uuid.size(), 36u) << uuid;
    }
    if (!scanner.results().empty())
    {
        EXPECT_TRUE(scanner.last_result().has_value());
    }
}

TEST_F(TestBtScan, test_start_stop)
{
    if (!has_powered_adapter())
        GTEST_SKIP() << "no powered bluetooth adapter";
    Scanner scanner("scanner");
    bool started = false;

    std::jthread scan_thread([&scanner, &started] { started = scanner.start(); });
    EXPECT_TRUE(scanner.wait_ready(Duration(time::sec(5))));
    EXPECT_TRUE(scanner.is_running());
    EXPECT_TRUE(scanner.stop());
    scan_thread.join();

    EXPECT_TRUE(started);
    EXPECT_FALSE(scanner.is_running());
}

TEST_F(TestBtScan, test_scan_twice)
{
    if (!has_powered_adapter())
        GTEST_SKIP() << "no powered bluetooth adapter";
    Scanner scanner("scanner");

    const bool first = scanner.scan_for(Duration(time::milli(600)));
    const bool second = scanner.scan_for(Duration(time::milli(600)));

    EXPECT_TRUE(first);
    EXPECT_TRUE(second);
}

TEST_F(TestBtScan, test_unknown_adapter)
{
    if (!has_powered_adapter())
        GTEST_SKIP() << "no powered bluetooth adapter";
    Scanner scanner("scanner");
    ASSERT_TRUE(scanner.set_adapter(99));

    const bool scanned = scanner.scan_for(Duration(time::milli(100)));

    EXPECT_FALSE(scanned);
    EXPECT_TRUE(scanner.results().empty());
    EXPECT_FALSE(scanner.last_result().has_value());
}

} // namespace test

#include <thread>

#include <gtest/gtest.h>

#include <sihd/bt/Scanner.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Stopwatch.hpp>
#include <sihd/util/Synchronizer.hpp>
#include <sihd/util/time.hpp>

namespace test
{
SIHD_NEW_LOGGER("test");
using namespace sihd::bt;
using namespace sihd::util;

// its first scan is stopped once its loop ended, before start() returns: where a late stop() lands
class LateStopScanner: public Scanner
{
    public:
        LateStopScanner(const std::string & name): Scanner(name) { _stop_reached.init_sync(2); }

        std::jthread stopper;

    protected:
        bool on_start() override
        {
            const bool scanned = Scanner::on_start();
            if (!stopper.joinable())
            {
                stopper = std::jthread([this] { this->stop(); });
                _stop_reached.sync(Duration(time::sec(5)));
            }
            return scanned;
        }

        bool on_stop() override
        {
            const bool stopped = Scanner::on_stop();
            _stop_reached.sync();
            return stopped;
        }

    private:
        Synchronizer _stop_reached;
};

class TestBtScan: public ::testing::Test
{
    protected:
        TestBtScan() { LoggerManager::stream(); }

        virtual ~TestBtScan() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}

        static bool has_powered_adapter() { return bluetooth_enabled().value_or(false); }
};

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

TEST_F(TestBtScan, test_stop_after_the_scan_ended_spares_the_next_scan)
{
    if (!has_powered_adapter())
        GTEST_SKIP() << "no powered bluetooth adapter";
    LateStopScanner scanner("scanner");
    ASSERT_TRUE(scanner.scan_for(Duration(time::milli(100))));
    scanner.stopper.join();

    Stopwatch stopwatch;
    const bool scanned = scanner.scan_for(Duration(time::milli(600)));
    const Duration elapsed = stopwatch.time();

    EXPECT_TRUE(scanned);
    EXPECT_GE(elapsed, Duration(time::milli(600)));
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

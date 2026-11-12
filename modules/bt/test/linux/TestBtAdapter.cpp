#include <gtest/gtest.h>

#include <sihd/bt/adapter.hpp>
#include <sihd/util/Logger.hpp>

#include "fake_bluez.hpp"

namespace test
{
using namespace sihd::bt;
using namespace sihd::util;

class TestBtAdapter: public ::testing::Test
{
    protected:
        TestBtAdapter() { LoggerManager::stream(); }

        virtual ~TestBtAdapter() { LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            if (!FakeDaemon::available())
                GTEST_SKIP() << "no dbus-daemon";
            ASSERT_TRUE(_daemon.start({bluez_service}));
            serve_adapter(_daemon, adapter_path, true);
            serve_adapter(_daemon, second_adapter_path, false);
        }

        FakeDaemon _daemon;
};

TEST_F(TestBtAdapter, test_list_adapters)
{
    _daemon.serve();

    auto adapters = list_adapters();

    ASSERT_TRUE(adapters.has_value()) << adapters.error().message;
    ASSERT_EQ(adapters->size(), 2u);
    const AdapterInfo & first = adapters->at(0);
    EXPECT_EQ(first.identifier, "hci0");
    EXPECT_EQ(first.address, "00:11:22:33:44:55");
    EXPECT_EQ(first.name, "fake");
    EXPECT_EQ(first.alias, "fake alias");
    EXPECT_TRUE(first.powered);
    EXPECT_FALSE(first.discoverable);
    EXPECT_TRUE(first.pairable);
    EXPECT_FALSE(first.discovering);
    EXPECT_EQ(adapters->at(1).identifier, "hci1");
    EXPECT_FALSE(adapters->at(1).powered);
}

TEST_F(TestBtAdapter, test_bluetooth_enabled)
{
    _daemon.serve();

    auto enabled = bluetooth_enabled();

    ASSERT_TRUE(enabled.has_value()) << enabled.error().message;
    EXPECT_TRUE(*enabled);
}

TEST_F(TestBtAdapter, test_bluetooth_disabled)
{
    _daemon.set_property(adapter_path, adapter_interface, "Powered", boolean(false));
    _daemon.serve();

    auto enabled = bluetooth_enabled();

    ASSERT_TRUE(enabled.has_value()) << enabled.error().message;
    EXPECT_FALSE(*enabled);
}

TEST_F(TestBtAdapter, test_set_adapter_properties)
{
    _daemon.serve();

    auto powered = set_adapter_powered(1, true);
    auto discoverable = set_adapter_discoverable(0, true);
    auto pairable = set_adapter_pairable(0, false);
    auto alias = set_adapter_alias(0, "renamed");
    auto adapters = list_adapters();

    EXPECT_TRUE(powered.has_value()) << powered.error().message;
    EXPECT_TRUE(discoverable.has_value()) << discoverable.error().message;
    EXPECT_TRUE(pairable.has_value()) << pairable.error().message;
    EXPECT_TRUE(alias.has_value()) << alias.error().message;
    ASSERT_TRUE(adapters.has_value()) << adapters.error().message;
    EXPECT_TRUE(adapters->at(1).powered);
    EXPECT_TRUE(adapters->at(0).discoverable);
    EXPECT_FALSE(adapters->at(0).pairable);
    EXPECT_EQ(adapters->at(0).alias, "renamed");
}

TEST_F(TestBtAdapter, test_unknown_adapter)
{
    _daemon.serve();

    auto powered = set_adapter_powered(2, true);

    ASSERT_FALSE(powered.has_value());
    EXPECT_EQ(powered.error().code, ErrorCode::not_found);
    EXPECT_EQ(powered.error().message, "no bluetooth adapter at index 2");
}

TEST_F(TestBtAdapter, test_bluez_error_code)
{
    _daemon.on(properties_interface, "Set", [this](const dbus::Message & call) {
        _daemon.fail(call, "org.bluez.Error.NotReady", "Resource Not Ready");
    });
    _daemon.serve();

    auto powered = set_adapter_powered(0, true);

    ASSERT_FALSE(powered.has_value());
    EXPECT_EQ(powered.error().code, ErrorCode::not_initialized);
    EXPECT_EQ(powered.error().message, "org.bluez.Error.NotReady: Resource Not Ready");
}

} // namespace test

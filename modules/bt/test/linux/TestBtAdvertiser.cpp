#include <cstdint>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/bt/Advertiser.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

#include "fake_bluez.hpp"

namespace test
{
using namespace sihd::bt;
using namespace sihd::util;

class TestBtAdvertiser: public ::testing::Test
{
    protected:
        TestBtAdvertiser(): _advertiser("advertiser") { LoggerManager::stream(); }

        virtual ~TestBtAdvertiser() { LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            if (!FakeDaemon::available())
                GTEST_SKIP() << "no dbus-daemon";
            ASSERT_TRUE(_daemon.start({bluez_service}));
            serve_adapter(_daemon, adapter_path, true);
            // bluez reads the advertisement back before RegisterAdvertisement answers
            _daemon.on(advertising_manager_interface, "RegisterAdvertisement", [this](const dbus::Message & call) {
                _advertisement_path = dbus::Iter(call).get<std::string>().value_or("");
                this->read_advertisement(call.sender());
                _daemon.reply(call, {});
                if (_release_after_register)
                    this->release(call.sender());
            });
            answer(_daemon, advertising_manager_interface, {"UnregisterAdvertisement"});
        }

        void read_advertisement(const std::string & sender)
        {
            dbus::Message request;
            auto built = request.properties_get_all(sender, _advertisement_path, advertisement_interface);
            ASSERT_TRUE(built.has_value()) << built.error().message;
            auto reply = request.call(_daemon.connection(), fake_timeout);
            ASSERT_TRUE(reply.has_value()) << reply.error().message;

            auto parsed = dbus::Iter(*reply).for_each_entry<std::string>(
                [this](const std::string & key, dbus::Iter & value) -> Result<void> {
                    if (key == "ManufacturerData")
                    {
                        return value.for_each_entry<uint16_t>([this](uint16_t id, dbus::Iter & data) {
                            _manufacturer_data[id] = data.get_array<uint8_t>().value_or(std::vector<uint8_t> {});
                            return Result<void> {};
                        });
                    }
                    if (key == "ServiceData")
                    {
                        return value.for_each_entry<std::string>([this](const std::string & uuid, dbus::Iter & data) {
                            _service_data[uuid] = data.get_array<uint8_t>().value_or(std::vector<uint8_t> {});
                            return Result<void> {};
                        });
                    }

                    if (auto text = value.get<std::string>())
                        _texts[key] = *text;
                    else if (auto array = value.get_array<std::string>())
                        _arrays[key] = *array;
                    return {};
                });
            ASSERT_TRUE(parsed.has_value()) << parsed.error().message;
        }

        // bluez drops an advertisement it cannot keep and tells its owner
        void release(const std::string & sender)
        {
            dbus::Message request;
            auto built = request.method_call(sender, _advertisement_path, advertisement_interface, "Release");
            ASSERT_TRUE(built.has_value()) << built.error().message;
            ASSERT_TRUE(request.send(_daemon.connection()).has_value());
        }

        void advertise_once()
        {
            std::jthread advertising([this] { _started = _advertiser.start(); });
            ASSERT_TRUE(_advertiser.wait_ready(Duration(time::sec(5))));
            _advertiser.stop();
        }

        FakeDaemon _daemon;
        bool _release_after_register = false;
        std::string _advertisement_path;
        std::map<std::string, std::string> _texts;
        std::map<std::string, std::vector<std::string>> _arrays;
        std::map<uint16_t, std::vector<uint8_t>> _manufacturer_data;
        std::map<std::string, std::vector<uint8_t>> _service_data;
        Advertiser _advertiser;
        bool _started = false;
};

TEST_F(TestBtAdvertiser, test_advertise)
{
    _daemon.serve();
    ASSERT_TRUE(_advertiser.set_advertisement(Advertisement {
        .local_name = "sihd",
        .service_uuids = {battery_uuid},
        .manufacturer_data = {{0xffff, {0x01, 0x02}}},
        .service_data = {{battery_uuid, {0x64}}},
        .connectable = true,
    }));

    ASSERT_NO_FATAL_FAILURE(this->advertise_once());
    _daemon.stop();

    EXPECT_TRUE(_started);
    EXPECT_EQ(_advertisement_path, "/org/sihd/bt/advertisement");
    EXPECT_EQ(_texts, (std::map<std::string, std::string> {{"Type", "peripheral"}, {"LocalName", "sihd"}}));
    EXPECT_EQ(_arrays.at("ServiceUUIDs"), std::vector<std::string> {battery_uuid});
    EXPECT_EQ(_manufacturer_data, (std::map<uint16_t, std::vector<uint8_t>> {{0xffff, {0x01, 0x02}}}));
    EXPECT_EQ(_service_data, (std::map<std::string, std::vector<uint8_t>> {{battery_uuid, {0x64}}}));
    EXPECT_TRUE(_daemon.called(advertising_manager_interface, "UnregisterAdvertisement"));
}

TEST_F(TestBtAdvertiser, test_broadcast_leaves_empty_fields_out)
{
    _daemon.serve();
    ASSERT_TRUE(_advertiser.set_advertisement(Advertisement {
        .local_name = "",
        .service_uuids = {},
        .manufacturer_data = {{0x004c, {0x02}}},
        .service_data = {},
        .connectable = false,
    }));

    ASSERT_NO_FATAL_FAILURE(this->advertise_once());
    _daemon.stop();

    EXPECT_EQ(_texts, (std::map<std::string, std::string> {{"Type", "broadcast"}}));
    EXPECT_TRUE(_arrays.empty());
    EXPECT_TRUE(_service_data.empty());
}

TEST_F(TestBtAdvertiser, test_released_by_bluez)
{
    _release_after_register = true;
    _daemon.serve();

    const bool started = _advertiser.start();
    _daemon.stop();

    EXPECT_FALSE(started);
    EXPECT_FALSE(_daemon.called(advertising_manager_interface, "UnregisterAdvertisement"));
}

TEST_F(TestBtAdvertiser, test_registration_refused)
{
    _daemon.on(advertising_manager_interface, "RegisterAdvertisement", [this](const dbus::Message & call) {
        _daemon.fail(call, "org.bluez.Error.NotPermitted", "Maximum advertisements reached");
    });
    _daemon.serve();

    const bool started = _advertiser.start();
    _daemon.stop();

    EXPECT_FALSE(started);
    EXPECT_FALSE(_daemon.called(advertising_manager_interface, "UnregisterAdvertisement"));
}

TEST_F(TestBtAdvertiser, test_advertisement_kept_while_advertising)
{
    _daemon.serve();

    std::jthread advertising([this] { _started = _advertiser.start(); });
    ASSERT_TRUE(_advertiser.wait_ready(Duration(time::sec(5))));
    const bool changed = _advertiser.set_advertisement(Advertisement {});
    _advertiser.stop();
    advertising.join();

    EXPECT_FALSE(changed);
}

} // namespace test

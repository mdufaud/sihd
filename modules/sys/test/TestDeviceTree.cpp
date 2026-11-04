#include <string_view>

#include <gtest/gtest.h>

#include <sihd/core/Channel.hpp>
#include <sihd/core/Core.hpp>
#include <sihd/core/DevPulsation.hpp>
#include <sihd/json/Json.hpp>
#include <sihd/sys/device_tree.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/build.hpp>

namespace test
{
using namespace sihd::util;
using namespace sihd::core;
using namespace sihd::sys;
using sihd::json::Json;

class TestDeviceTree: public ::testing::Test
{
    protected:
        TestDeviceTree() { LoggerManager::stream(); }

        virtual ~TestDeviceTree() { LoggerManager::clear_loggers(); }

        Result<void> load(std::string_view text)
        {
            auto tree = Json::parse(text);
            EXPECT_TRUE(tree.has_value());
            return device_tree::load(_core, tree.value_or(Json()));
        }

        void expect_loaded(std::string_view text)
        {
            const Result<void> loaded = this->load(text);
            ASSERT_TRUE(loaded.has_value()) << loaded.error().message;
        }

        void expect_error(std::string_view text, ErrorCode code, const std::string & message)
        {
            const Result<void> loaded = this->load(text);
            ASSERT_FALSE(loaded.has_value());
            EXPECT_EQ(loaded.error().code, code);
            EXPECT_EQ(loaded.error().message, message);
        }

        Core _core;
};

TEST_F(TestDeviceTree, test_device_tree_loads_channels_devices_and_links)
{
    if constexpr (build::is_statically_linked && !SIHD_FACTORY_TABLE)
        GTEST_SKIP() << "no factory resolution in this build";

    this->expect_loaded(R"({
        "channels": [
            {"name": "run", "type": "bool"},
            {"name": "packets", "type": "byte", "size": 0, "capacity": 64}
        ],
        "devices": [
            {"class": "Core", "name": "consumer", "links": {"count": "..stats.packets_count"}},
            {"class": "DevStats", "name": "stats", "conf": {"period": 50}, "links": {"packets": "..packets"}},
            {"class": "Core", "name": "group", "devices": [
                {"class": "DevPulsation", "name": "pulse", "conf": {"frequency": 10}, "links": {"activate": "..pulse_on.value"}},
                {"class": "Core", "name": "pulse_on", "channels": [{"name": "value", "type": "bool"}]}
            ]}
        ]
    })");

    ASSERT_TRUE(_core.init());
    ASSERT_TRUE(_core.start());

    Channel *packets = _core.find_channel("packets").value_or(nullptr);
    ASSERT_NE(packets, nullptr);
    EXPECT_EQ(packets->capacity(), 64u);
    EXPECT_EQ(_core.find_channel("run").value()->data_type(), TYPE_BOOL);
    EXPECT_EQ(_core.find_channel("consumer.count").value_or(nullptr),
              _core.find_channel("stats.packets_count").value_or(nullptr));
    EXPECT_NE(_core.find<DevPulsation>("group.pulse"), nullptr);
    EXPECT_TRUE(_core.stop());
}

TEST_F(TestDeviceTree, test_device_tree_refuses_unknown_keys)
{
    this->expect_error(R"({"device": []})", ErrorCode::invalid_argument, "core: unknown key 'device'");
    this->expect_error(R"({"devices": [{"class": "Core", "name": "a", "link": {}}]})",
                       ErrorCode::invalid_argument,
                       "core: unknown key 'link'");
    this->expect_error(R"({"channels": [{"name": "a", "type": "int", "sizes": 2}]})",
                       ErrorCode::invalid_argument,
                       "core: unknown key 'sizes'");
}

TEST_F(TestDeviceTree, test_device_tree_refuses_bad_values)
{
    this->expect_error(R"({"channels": [{"name": "a", "type": "nope"}]})",
                       ErrorCode::invalid_argument,
                       "core: could not add channel 'a' of type 'nope'");
    this->expect_error(R"({"channels": [{"name": "b", "type": "int", "size": -1}]})",
                       ErrorCode::invalid_argument,
                       "core: 'size' must be a positive integer");
    this->expect_error(R"({"links": {"a": 1}})", ErrorCode::invalid_argument, "core: link 'a' must be a path string");
    this->expect_error(R"({"devices": [{"name": "a"}]})",
                       ErrorCode::invalid_argument,
                       "core: 'class' must be a string");
}

TEST_F(TestDeviceTree, test_device_tree_refuses_unknown_class_and_taken_name)
{
    if constexpr (build::is_statically_linked && !SIHD_FACTORY_TABLE)
        GTEST_SKIP() << "no factory resolution in this build";

    const Result<void> unknown = this->load(R"({"devices": [{"class": "NoSuchDevice", "name": "a"}]})");
    ASSERT_FALSE(unknown.has_value());
    EXPECT_EQ(unknown.error().code, ErrorCode::not_found);

    this->expect_error(R"({"devices": [{"class": "Core", "name": "b"}, {"class": "Core", "name": "b"}]})",
                       ErrorCode::already_exists,
                       "core: child 'b' already exists");
    this->expect_error(R"({"devices": [{"class": "MemRecorder", "name": "c"}]})",
                       ErrorCode::invalid_argument,
                       "core: class 'MemRecorder' is not a device");
}

TEST_F(TestDeviceTree, test_device_tree_reports_a_conf_error)
{
    if constexpr (build::is_statically_linked && !SIHD_FACTORY_TABLE)
        GTEST_SKIP() << "no factory resolution in this build";

    const Result<void> loaded = this->load(R"({"devices": [{"class": "DevStats", "name": "a", "conf": {"nope": 1}}]})");

    ASSERT_FALSE(loaded.has_value());
    EXPECT_NE(_core.get_child("a"), nullptr);
}

} // namespace test

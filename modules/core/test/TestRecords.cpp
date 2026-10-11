#include <chrono>
#include <memory>
#include <thread>
#include <wait_for.hpp>

#include <gtest/gtest.h>

#include <sihd/core/ChannelWaiter.hpp>
#include <sihd/core/Core.hpp>
#include <sihd/core/DevPlayer.hpp>
#include <sihd/core/DevRecorder.hpp>
#include <sihd/core/MemRecorder.hpp>
#include <sihd/json/Json.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::core;
using namespace sihd::util;
class TestRecords: public ::testing::Test
{
    protected:
        TestRecords() { sihd::util::LoggerManager::stream(); }

        virtual ~TestRecords() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestRecords, test_records_dev_player)
{
    Core core;

    Channel *int_channel = core.add_channel("int_channel", sihd::util::TYPE_INT, 2);
    Channel *bool_channel = core.add_channel("bool_channel", sihd::util::TYPE_BOOL, 4);

    MemRecorder mem_recorder("mem_recorder", &core);
    mem_recorder.set_parent_ownership(false);
    EXPECT_TRUE(mem_recorder.set_stop_providing_when_empty(true));

    DevPlayer dev_replayer("dev_replayer", &core);
    dev_replayer.set_parent_ownership(false);
    EXPECT_TRUE(dev_replayer.set_conf_str("provider", "..mem_recorder"));
    EXPECT_TRUE(dev_replayer.set_conf({{"provider", "..mem_recorder"},
                                       {"provider_wait_time", 1},
                                       {"alias",
                                        {
                                            "int=..int_channel",
                                            "bool=..bool_channel",
                                        }}}));

    sihd::util::ArrInt arr_int = {10, 0};
    sihd::util::ArrBool arr_bool = {false, false, false, false};
    mem_recorder.add_record("int", sihd::util::Timestamp(sihd::util::time::milli(5)), &arr_int);
    mem_recorder.add_record("bool", sihd::util::Timestamp(sihd::util::time::milli(10)), &arr_bool);
    arr_int[1] = 20;
    mem_recorder.add_record("int", sihd::util::Timestamp(sihd::util::time::milli(15)), &arr_int);
    arr_bool[0] = true;
    mem_recorder.add_record("bool", sihd::util::Timestamp(sihd::util::time::milli(20)), &arr_bool);
    arr_bool[2] = true;
    arr_bool[3] = true;
    mem_recorder.add_record("bool", sihd::util::Timestamp(sihd::util::time::milli(25)), &arr_bool);

    std::cout << core.tree_desc_str() << std::endl;
    EXPECT_TRUE(core.init());
    std::cout << core.tree_desc_str() << std::endl;

    Channel *end = dev_replayer.get_channel("end").value_or(nullptr);
    ASSERT_NE(end, nullptr);
    Channel *play = dev_replayer.get_channel("play").value_or(nullptr);
    ASSERT_NE(play, nullptr);

    EXPECT_TRUE(core.start());

    ChannelWaiter waiter(end);

    SIHD_LOG(debug, "Playing");
    play->write(0, true);

    SIHD_LOG(debug, "Waiting the end");
    // generous bound: replay is clock driven and qemu emulation runs slower
    EXPECT_TRUE(waiter.wait_for_nb(sihd::util::Duration(sihd::util::time::milli(150)), 1));

    EXPECT_TRUE(core.stop());

    SIHD_LOG(debug, "Ended");

    EXPECT_EQ(int_channel->read<int>(0), 10);
    EXPECT_EQ(int_channel->read<int>(1), 20);

    EXPECT_EQ(bool_channel->read<bool>(0), true);
    EXPECT_EQ(bool_channel->read<bool>(1), false);
    EXPECT_EQ(bool_channel->read<bool>(2), true);
    EXPECT_EQ(bool_channel->read<bool>(3), true);
}

TEST_F(TestRecords, test_records_dev_recorder)
{
#if defined(__SIHD_EMSCRIPTEN__)
    // emscripten clock granularity is coarse: space recorded writes so their
    // timestamps stay strictly increasing instead of tying
    const auto tick = [] {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    };
#else
    const auto tick = [] {
    };
#endif
    Core core;
    Channel *int_channel = core.add_channel("int_channel", sihd::util::TYPE_INT, 2);
    Channel *bool_channel = core.add_channel("bool_channel", sihd::util::TYPE_BOOL, 4);
    bool_channel->set_write_on_change(false);
    Channel *double_channel = core.add_channel("double_channel", sihd::util::TYPE_DOUBLE, 1);
    // unused channel
    core.add_channel("char_channel", sihd::util::TYPE_CHAR, 10);

    MemRecorder mem_recorder("mem_recorder", &core);
    mem_recorder.set_parent_ownership(false);

    DevRecorder dev_recorder("dev_recorder", &core);
    dev_recorder.set_parent_ownership(false);
    // get channels from core with json
    EXPECT_TRUE(dev_recorder.set_conf({{"handler", "..mem_recorder"},
                                       {"record",
                                        {
                                            "int=..int_channel",
                                            "bool=..bool_channel",
                                            "double=..double_channel",
                                        }}}));
    // or this
    (void)dev_recorder.set_conf_str("record", "char=..char_channel");

    std::cout << core.tree_desc_str() << std::endl;
    EXPECT_TRUE(core.init());
    std::cout << core.tree_desc_str() << std::endl;
    EXPECT_TRUE(core.start());

    // write multiple values
    int_channel->write(0, 10);
    tick();
    int_channel->write(1, 20);
    tick();

    // first write should occur because of set_write_on_change(false)
    bool_channel->write(0, false);
    tick();
    bool_channel->write(1, true);
    tick();
    bool_channel->write(2, true);
    tick();
    bool_channel->write(3, true);
    tick();

    // should be only one write (same)
    double_channel->write(0, 12.34);
    double_channel->write(0, 12.34);
    double_channel->write(0, 12.34);
    double_channel->write(0, 12.34);
    double_channel->write(0, 12.34);

    EXPECT_TRUE(core.stop());

    // verify recording
    const MapListRecordedValues & map = mem_recorder.make_recorded_values();
    const SortedRecordedValues & sorted_map = mem_recorder.sorted_recorded_values();

    sihd::util::Timestamp last_timestamp(-1);
    for (const auto & pair : sorted_map)
    {
        if (last_timestamp < 0)
            last_timestamp = pair.second.timestamp;
        else
            EXPECT_GT(pair.second.timestamp, last_timestamp);
    }

    SIHD_LOG(debug, "Printing dev recorder total values");
    std::cout << mem_recorder.hexdump_recorded_values(map) << std::endl;
    std::cout << mem_recorder.hexdump_records() << std::endl;

    EXPECT_TRUE(map.find("int") != map.end());
    EXPECT_TRUE(map.find("bool") != map.end());
    EXPECT_TRUE(map.find("double") != map.end());
    EXPECT_TRUE(map.find("char") == map.end());

    EXPECT_EQ(map.at("bool").size(), 4u);
    EXPECT_EQ(map.at("double").size(), 1u);

    const ListRecordedValues & lst = map.at("int");
    EXPECT_EQ(lst.size(), 2u);

    // timestamps
    EXPECT_TRUE(lst.front().first > 0);
    EXPECT_TRUE(lst.back().first > 0);
    EXPECT_TRUE(lst.back().first > lst.front().first);

    // values
    sihd::util::IArrayShared front_array = lst.front().second;
    sihd::util::ArrInt *front_int_array = dynamic_cast<sihd::util::ArrInt *>(front_array.get());
    EXPECT_NE(front_int_array, nullptr);
    if (front_int_array != nullptr)
    {
        EXPECT_EQ(front_int_array->at(0), 10);
        EXPECT_EQ(front_int_array->at(1), 0);
    }

    sihd::util::IArrayShared back_array = lst.back().second;
    sihd::util::ArrInt *back_int_array = dynamic_cast<sihd::util::ArrInt *>(back_array.get());
    EXPECT_NE(back_int_array, nullptr);
    if (back_int_array != nullptr)
    {
        EXPECT_EQ(back_int_array->at(0), 10);
        EXPECT_EQ(back_int_array->at(1), 20);
    }

    // channel array and back_array must be in the same state
    EXPECT_TRUE(back_array->is_bytes_equal(*int_channel->array()));
}

TEST_F(TestRecords, test_records_recorder_handler_not_found)
{
    Core core;
    core.add_channel("int_channel", sihd::util::TYPE_INT, 2);
    MemRecorder mem_recorder("mem_recorder", &core);
    mem_recorder.set_parent_ownership(false);

    DevRecorder dev_recorder("dev_recorder", &core);
    dev_recorder.set_parent_ownership(false);
    (void)dev_recorder.set_conf_str("handler", "..no_such_handler");

    EXPECT_FALSE(core.init());
}

TEST_F(TestRecords, test_records_recorder_channel_not_found)
{
    Core core;
    core.add_channel("int_channel", sihd::util::TYPE_INT, 2);
    MemRecorder mem_recorder("mem_recorder", &core);
    mem_recorder.set_parent_ownership(false);

    DevRecorder dev_recorder("dev_recorder", &core);
    dev_recorder.set_parent_ownership(false);
    ASSERT_TRUE(dev_recorder.set_conf_str("handler", "..mem_recorder"));
    ASSERT_TRUE(dev_recorder.add_record_channel("int=..no_such_channel"));

    EXPECT_TRUE(core.init());
    EXPECT_FALSE(core.start());
}

TEST_F(TestRecords, test_records_recorder_remove_recorded_channel)
{
    Core core;
    Channel *int_channel = core.add_channel("int_channel", sihd::util::TYPE_INT, 2);
    Channel *bool_channel = core.add_channel("bool_channel", sihd::util::TYPE_BOOL, 2);
    MemRecorder mem_recorder("mem_recorder", &core);
    mem_recorder.set_parent_ownership(false);

    DevRecorder dev_recorder("dev_recorder", &core);
    dev_recorder.set_parent_ownership(false);
    ASSERT_TRUE(dev_recorder.set_conf_str("handler", "..mem_recorder"));
    ASSERT_TRUE(dev_recorder.add_record_channel("int=..int_channel"));
    ASSERT_TRUE(dev_recorder.add_record_channel("bool=..bool_channel"));
    EXPECT_TRUE(dev_recorder.remove_recorded_channel("bool"));

    EXPECT_TRUE(core.init());
    EXPECT_TRUE(core.start());

    int_channel->write(0, 7);
    bool_channel->write(0, true);

    EXPECT_TRUE(core.stop());

    const MapListRecordedValues & map = mem_recorder.make_recorded_values();
    ASSERT_TRUE(map.find("int") != map.end());
    EXPECT_TRUE(map.find("bool") == map.end());
    ASSERT_EQ(map.at("int").size(), 1u);
    sihd::util::ArrInt *recorded = dynamic_cast<sihd::util::ArrInt *>(map.at("int").front().second.get());
    ASSERT_NE(recorded, nullptr);
    EXPECT_EQ(recorded->at(0), 7);
}

TEST_F(TestRecords, test_records_player_provider_not_found)
{
    Core core;
    DevPlayer dev_player("dev_player", &core);
    dev_player.set_parent_ownership(false);
    EXPECT_TRUE(dev_player.set_conf_str("provider", "..no_such_provider"));

    EXPECT_TRUE(core.init());
    EXPECT_FALSE(core.start());
}

TEST_F(TestRecords, test_records_player_pause_resume)
{
    Core core;
    Channel *int_channel = core.add_channel("int_channel", sihd::util::TYPE_INT, 2);

    MemRecorder mem_recorder("mem_recorder", &core);
    mem_recorder.set_parent_ownership(false);
    EXPECT_TRUE(mem_recorder.set_stop_providing_when_empty(true));

    DevPlayer dev_replayer("dev_replayer", &core);
    dev_replayer.set_parent_ownership(false);
    EXPECT_TRUE(dev_replayer.set_conf({{"provider", "..mem_recorder"},
                                       {"provider_wait_time", 1},
                                       {"alias",
                                        {
                                            "int=..int_channel",
                                        }}}));

    sihd::util::ArrInt arr_int = {1, 0};
    mem_recorder.add_record("int", sihd::util::Timestamp(sihd::util::time::milli(5)), &arr_int);
    arr_int[0] = 2;
    mem_recorder.add_record("int", sihd::util::Timestamp(sihd::util::time::milli(40)), &arr_int);
    arr_int[0] = 3;
    mem_recorder.add_record("int", sihd::util::Timestamp(sihd::util::time::milli(80)), &arr_int);
    arr_int[0] = 4;
    mem_recorder.add_record("int", sihd::util::Timestamp(sihd::util::time::milli(120)), &arr_int);

    EXPECT_TRUE(core.init());

    Channel *end = dev_replayer.get_channel("end").value_or(nullptr);
    Channel *play = dev_replayer.get_channel("play").value_or(nullptr);
    ASSERT_NE(end, nullptr);
    ASSERT_NE(play, nullptr);

    EXPECT_TRUE(core.start());

    ChannelWaiter end_waiter(end);

    play->write(0, true);
    EXPECT_TRUE(wait_for([&] { return int_channel->read<int>(0) == 1; }, std::chrono::milliseconds(1000)));

    play->write(0, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    const int paused_value = int_channel->read<int>(0);
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    EXPECT_EQ(int_channel->read<int>(0), paused_value);

    play->write(0, true);
    EXPECT_TRUE(end_waiter.wait_for_nb(sihd::util::Duration(sihd::util::time::milli(500)), 1));
    EXPECT_EQ(int_channel->read<int>(0), 4);

    EXPECT_TRUE(core.stop());
}

TEST_F(TestRecords, test_records_mem_recorder_provider)
{
    MemRecorder rec("rec");
    EXPECT_TRUE(rec.empty());

    sihd::util::ArrInt arr = {1, 2};
    rec.add_record("b", sihd::util::Timestamp(sihd::util::time::milli(2)), &arr);
    rec.add_record("a", sihd::util::Timestamp(sihd::util::time::milli(1)), &arr);
    EXPECT_FALSE(rec.empty());

    PlayableRecord out;
    ASSERT_TRUE(rec.provide(&out));
    EXPECT_EQ(out.name, "a");
    ASSERT_TRUE(rec.provide(&out));
    EXPECT_EQ(out.name, "b");
    EXPECT_FALSE(rec.provide(&out));

    rec.clear();
    EXPECT_TRUE(rec.empty());

    std::vector<PlayableRecord> records;
    records.push_back(PlayableRecord {"c",
                                      sihd::util::Timestamp(sihd::util::time::milli(3)),
                                      std::make_shared<sihd::util::ArrInt>(arr)});
    records.push_back(PlayableRecord {"d",
                                      sihd::util::Timestamp(sihd::util::time::milli(4)),
                                      std::make_shared<sihd::util::ArrInt>(arr)});
    rec.add_records(records);
    ASSERT_TRUE(rec.provide(&out));
    EXPECT_EQ(out.name, "c");
    ASSERT_TRUE(rec.provide(&out));
    EXPECT_EQ(out.name, "d");
    EXPECT_FALSE(rec.provide(&out));

    rec.add_record("e", sihd::util::Timestamp(sihd::util::time::milli(5)), &arr);
    EXPECT_NE(rec.hexdump_records().find("e"), std::string::npos);
}
} // namespace test

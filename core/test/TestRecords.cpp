#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <sihd/util/Array.hpp>
#include <sihd/util/Logger.hpp>

#include <sihd/core/ChannelWaiter.hpp>
#include <sihd/core/Core.hpp>
#include <sihd/core/DevPlayer.hpp>
#include <sihd/core/DevRecorder.hpp>
#include <sihd/core/MemRecorder.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::core;
using namespace sihd::util;
class TestRecords: public ::testing::Test
{
    protected:
        TestRecords() { sihd::util::LoggerManager::basic(); }

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
    mem_recorder.add_record("int", sihd::util::time::milli(5), &arr_int);
    mem_recorder.add_record("bool", sihd::util::time::milli(10), &arr_bool);
    arr_int[1] = 20;
    mem_recorder.add_record("int", sihd::util::time::milli(15), &arr_int);
    arr_bool[0] = true;
    mem_recorder.add_record("bool", sihd::util::time::milli(20), &arr_bool);
    arr_bool[2] = true;
    arr_bool[3] = true;
    mem_recorder.add_record("bool", sihd::util::time::milli(25), &arr_bool);

    std::cout << core.tree_desc_str() << std::endl;
    EXPECT_TRUE(core.init());
    std::cout << core.tree_desc_str() << std::endl;

    Channel *end = dev_replayer.get_channel("end");
    ASSERT_NE(end, nullptr);
    Channel *play = dev_replayer.get_channel("play");
    ASSERT_NE(play, nullptr);

    EXPECT_TRUE(core.start());

    ChannelWaiter waiter(end);

    SIHD_LOG(debug, "Playing");
    play->write(0, true);

    SIHD_LOG(debug, "Waiting the end");
    EXPECT_TRUE(waiter.wait_for_nb(sihd::util::time::milli(50), 1));

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
    dev_recorder.set_conf_str("record", "char=..char_channel");

    std::cout << core.tree_desc_str() << std::endl;
    EXPECT_TRUE(core.init());
    std::cout << core.tree_desc_str() << std::endl;
    EXPECT_TRUE(core.start());

    // write multiple values
    int_channel->write(0, 10);
    int_channel->write(1, 20);

    // first write should occur because of set_write_on_change(false)
    bool_channel->write(0, false);
    bool_channel->write(1, true);
    bool_channel->write(2, true);
    bool_channel->write(3, true);

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

    time_t last_timestamp = -1;
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
} // namespace test

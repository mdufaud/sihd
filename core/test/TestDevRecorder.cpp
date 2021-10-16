#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/core/DevRecorder.hpp>
#include <sihd/core/Core.hpp>

namespace test
{
    LOGGER;
    using namespace sihd::core;
    class TestDevRecorder:   public ::testing::Test
    {
        protected:
            TestDevRecorder()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestDevRecorder()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }
    };

    TEST_F(TestDevRecorder, test_devrecorder)
    {
        Core core;
        Channel *int_channel = core.add_channel("int_channel", sihd::util::DINT, 2);
        Channel *bool_channel = core.add_channel("bool_channel", sihd::util::DBOOL, 4);
        Channel *double_channel = core.add_channel("double_channel", sihd::util::DDOUBLE, 1);
        core.add_channel("char_channel", sihd::util::DCHAR, 10);

        bool_channel->set_write_on_change(false);

        DevRecorder recorder("recorder", &core);
        recorder.set_parent_ownership(false);
        // get channels from core with json
        recorder.set_conf({
            {"record", {
                "..int_channel",
                "..bool_channel",
                "..double_channel",
            }}
        });
        // or this
        recorder.set_conf_str("record", "..char_channel");

        std::cout << core.get_tree_desc_str() << std::endl;

        EXPECT_TRUE(core.init());
        EXPECT_TRUE(core.start());

        // clear values
        EXPECT_NE(recorder.get_channel("clear"), nullptr);
        int_channel->write(0, 123456);
        recorder.get_channel("clear")->write(0, true);

        // write multiple values
        int_channel->write(0, 10);
        int_channel->write(1, 20);

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
        const DevRecorder::map_recorded_channels & map = recorder.recorded_channels_values();

        LOG(debug, "Printing dev recorder total values");
        std::cout << DevRecorder::to_string(map);

        EXPECT_TRUE(map.find("..int_channel") != map.end());
        EXPECT_TRUE(map.find("..bool_channel") != map.end());
        EXPECT_TRUE(map.find("..double_channel") != map.end());
        EXPECT_TRUE(map.find("..char_channel") == map.end());

        EXPECT_EQ(map.at("..bool_channel").size(), 4u);
        EXPECT_EQ(map.at("..double_channel").size(), 1u);

        const DevRecorder::lst_recorded_values & lst = map.at("..int_channel");
        EXPECT_EQ(lst.size(), 2u);

        // timestamps
        EXPECT_TRUE(lst.front().first > 0);
        EXPECT_TRUE(lst.back().first > 0);
        EXPECT_TRUE(lst.back().first > lst.front().first);

        // values
        sihd::util::IArray *front_array = lst.front().second;
        sihd::util::ArrInt *front_int_array = dynamic_cast<sihd::util::ArrInt *>(front_array);
        EXPECT_NE(front_int_array, nullptr);
        if (front_int_array != nullptr)
        {
            EXPECT_EQ(front_int_array->at(0), 10);
            EXPECT_EQ(front_int_array->at(1), 0);
        }

        sihd::util::IArray *back_array = lst.back().second;
        sihd::util::ArrInt *back_int_array = dynamic_cast<sihd::util::ArrInt *>(back_array);
        EXPECT_NE(back_int_array, nullptr);
        if (back_int_array != nullptr)
        {
            EXPECT_EQ(back_int_array->at(0), 10);
            EXPECT_EQ(back_int_array->at(1), 20);
        }

        // channel array and back_array must be in the same state
        EXPECT_TRUE(back_array->is_equal(*int_channel->array()));
    }
}
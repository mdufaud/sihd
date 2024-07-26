#include <gtest/gtest.h>

#include <sihd/core/ChannelWaiter.hpp>
#include <sihd/core/Core.hpp>
#include <sihd/core/DevFilter.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/term.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::core;
using namespace sihd::util;
class TestDevFilter: public ::testing::Test
{
    protected:
        TestDevFilter() { sihd::util::LoggerManager::basic(); }

        virtual ~TestDevFilter() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestDevFilter, test_devfilter_equal)
{
    Core core;

    DevFilter *dev_ptr = core.add_child<DevFilter>("filter");
    ASSERT_TRUE(dev_ptr->set_conf_str("filter_equal", "in=..in_channel;out=..out_channel;trigger=1:10;write=2:15"));

    core.add_channel("in_channel", "int", 3);
    core.add_channel("out_channel", "int", 3);

    ASSERT_TRUE(core.init());
    ASSERT_TRUE(core.start());

    Channel *in_channel = core.get_channel("in_channel");
    Channel *out_channel = core.get_channel("out_channel");
    ASSERT_NE(in_channel, nullptr);
    ASSERT_NE(out_channel, nullptr);

    SIHD_LOG(debug, "Testing filter_equal filter");
    EXPECT_EQ(out_channel->read<int>(2), 0);
    in_channel->write<int>(0, 10);
    EXPECT_EQ(out_channel->read<int>(2), 0);
    in_channel->write<int>(1, 10);
    EXPECT_EQ(out_channel->read<int>(2), 15);
}

TEST_F(TestDevFilter, test_devfilter_superior)
{
    Core core;

    DevFilter *dev_ptr = core.add_child<DevFilter>("filter");
    dev_ptr->set_filter(DevFilter::Rule(DevFilter::Superior)
                            .in("..in_channel")
                            .trigger<int>(1, 10)
                            .out("..out_channel")
                            .write<int>(2, 15));
    ASSERT_TRUE(dev_ptr->set_conf_str("filter_superior_equal", "in=..in_channel;out=..out_channel;trigger=0x200"));

    core.add_channel("in_channel", "int", 3);
    core.add_channel("out_channel", "int", 3);

    ASSERT_TRUE(core.init());
    ASSERT_TRUE(core.start());

    Channel *in_channel = core.get_channel("in_channel");
    Channel *out_channel = core.get_channel("out_channel");
    ASSERT_NE(in_channel, nullptr);
    ASSERT_NE(out_channel, nullptr);

    SIHD_LOG(debug, "Testing filter_superior filter");
    EXPECT_EQ(out_channel->read<int>(2), 0);
    in_channel->write<int>(1, 10);
    EXPECT_EQ(out_channel->read<int>(2), 0);
    in_channel->write<int>(1, 11);
    EXPECT_EQ(out_channel->read<int>(2), 15);

    SIHD_LOG(debug, "Testing filter_superior_equal filter");
    EXPECT_EQ(out_channel->read<int>(0), 0);
    in_channel->write<int>(0, 199);
    EXPECT_EQ(out_channel->read<int>(0), 0);
    in_channel->write<int>(0, 0x200);
    EXPECT_EQ(out_channel->read<int>(0), 0x200);
}

TEST_F(TestDevFilter, test_devfilter_inferior)
{
    Core core;

    DevFilter *dev_ptr = core.add_child<DevFilter>("filter");
    ASSERT_TRUE(dev_ptr->set_conf_str("filter_inferior", "in=..in_channel;out=..out_channel;trigger=1:10;write=2:15"));
    ASSERT_TRUE(dev_ptr->set_conf_str("filter_inferior_equal", "in=..in_channel;out=..out_channel;trigger=200"));

    core.add_channel("in_channel", "int", 3);
    core.add_channel("out_channel", "int", 3);

    ASSERT_TRUE(core.init());
    ASSERT_TRUE(core.start());

    Channel *in_channel = core.get_channel("in_channel");
    Channel *out_channel = core.get_channel("out_channel");
    ASSERT_NE(in_channel, nullptr);
    ASSERT_NE(out_channel, nullptr);

    SIHD_LOG(debug, "Testing filter_inferior filter");
    EXPECT_EQ(out_channel->read<int>(2), 0);
    in_channel->write<int>(1, 10);
    EXPECT_EQ(out_channel->read<int>(2), 0);
    in_channel->write<int>(1, 9);
    EXPECT_EQ(out_channel->read<int>(2), 15);

    SIHD_LOG(debug, "Testing filter_inferior_equal filter");
    EXPECT_EQ(out_channel->read<int>(0), 0);
    in_channel->write<int>(0, 201);
    EXPECT_EQ(out_channel->read<int>(0), 0);
    in_channel->write<int>(0, 200);
    EXPECT_EQ(out_channel->read<int>(0), 200);
}

TEST_F(TestDevFilter, test_devfilter_float)
{
    Core core;

    DevFilter *dev_ptr = core.add_child<DevFilter>("filter");
    dev_ptr->set_filter(DevFilter::Rule(DevFilter::Equal)
                            .in("..in_channel")
                            .trigger(0, 3.14f)
                            .out("..out_channel")
                            .write(0, 0x1)
                            .delay(time::ms(5)));
    ASSERT_TRUE(dev_ptr->set_conf_str("filter_equal",
                                      "in=..in_channel;out=..out_channel;trigger=6.28f;write=0b101;delay=0.01"));

    core.add_channel("in_channel", "float");
    core.add_channel("out_channel", "int");

    ASSERT_TRUE(core.init());
    ASSERT_TRUE(core.start());

    Channel *in_channel = core.get_channel("in_channel");
    Channel *out_channel = core.get_channel("out_channel");
    ASSERT_NE(in_channel, nullptr);
    ASSERT_NE(out_channel, nullptr);

    SIHD_LOG(debug, "Testing float filter");
    EXPECT_EQ(out_channel->read<int>(0), 0);
    in_channel->write<float>(0, 3.13f);
    EXPECT_EQ(out_channel->read<int>(0), 0);
    in_channel->write<float>(0, 3.15f);
    EXPECT_EQ(out_channel->read<int>(0), 0);

    ChannelWaiter waiter(out_channel);

    // delay -> 5ms
    EXPECT_EQ(out_channel->read<int>(0), 0);
    in_channel->write<float>(0, 3.14f);
    EXPECT_FALSE(waiter.prev_wait_for(std::chrono::milliseconds(3)));
    EXPECT_EQ(out_channel->read<int>(0), 0);
    EXPECT_TRUE(waiter.prev_wait_for(std::chrono::milliseconds(10)));
    EXPECT_EQ(out_channel->read<int>(0), 1);

    // delay -> 0.01 = 10ms
    EXPECT_EQ(out_channel->read<int>(0), 1);
    in_channel->write<float>(0, 6.28f);
    EXPECT_FALSE(waiter.prev_wait_for(std::chrono::milliseconds(3)));
    EXPECT_EQ(out_channel->read<int>(0), 1);
    EXPECT_TRUE(waiter.prev_wait_for(std::chrono::milliseconds(10)));
    EXPECT_EQ(out_channel->read<int>(0), 0b101);
}

TEST_F(TestDevFilter, test_devfilter_byte)
{
    Core core;

    DevFilter *dev_ptr = core.add_child<DevFilter>("filter");
    ASSERT_TRUE(dev_ptr->set_conf_str("filter_byte_and", "in=..in_channel;out=..out_channel;trigger=0b1"));
    ASSERT_TRUE(dev_ptr->set_conf_str("filter_byte_and",
                                      "in=..in_channel;out=..out_channel;trigger=1:0b1;write=1:;match=false"));

    core.add_channel("in_channel", "int", 2);
    core.add_channel("out_channel", "int", 2);

    ASSERT_TRUE(core.init());
    ASSERT_TRUE(core.start());

    Channel *in_channel = core.get_channel("in_channel");
    Channel *out_channel = core.get_channel("out_channel");
    ASSERT_NE(in_channel, nullptr);
    ASSERT_NE(out_channel, nullptr);

    SIHD_LOG(debug, "Testing byte_and filter");
    EXPECT_EQ(out_channel->read<int>(0), 0);
    in_channel->write<int>(0, 1);
    // only writes uneven
    EXPECT_EQ(out_channel->read<int>(0), 1);
    in_channel->write<int>(0, 2);
    EXPECT_EQ(out_channel->read<int>(0), 1);
    in_channel->write<int>(0, 3);
    EXPECT_EQ(out_channel->read<int>(0), 3);

    SIHD_LOG(debug, "Testing byte_and with no match filter");
    EXPECT_EQ(out_channel->read<int>(1), 0);
    in_channel->write<int>(1, 1);
    EXPECT_EQ(out_channel->read<int>(1), 0);
    in_channel->write<int>(1, 2);
    // only writes even
    EXPECT_EQ(out_channel->read<int>(1), 2);
    in_channel->write<int>(1, 3);
    EXPECT_EQ(out_channel->read<int>(1), 2);
    in_channel->write<int>(1, 4);
    EXPECT_EQ(out_channel->read<int>(1), 4);
}
} // namespace test

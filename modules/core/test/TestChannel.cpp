#include <clock_helper.hpp>

#include <gtest/gtest.h>

#include <sihd/core/Channel.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

namespace test
{
SIHD_NEW_LOGGER("sihd::test");
using namespace sihd::util;
using namespace sihd::core;
class TestChannel: public ::testing::Test,
                   public IHandler<Channel *>
{
    protected:
        TestChannel() { LoggerManager::stream(); }

        virtual ~TestChannel() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}

        void handle(Channel *channel)
        {
            SIHD_TRACEV(channel->array()->data_type_str());
            if (channel->array()->data_type() == TYPE_INT)
            {
                const ArrInt *arr_int = dynamic_cast<const ArrInt *>(channel->array());
                ASSERT_NE(arr_int, nullptr);
                const int *c_arr_int = arr_int->data();
                EXPECT_NO_THROW(_at_val = arr_int->at(0); _c_arr_val = c_arr_int[0];
                                _read_val = channel->read<int>(0););
            }
            _notified[channel] += 1;
        }

        std::map<Channel *, int> _notified;
        int _at_val = 0;
        int _c_arr_val = 0;
        int _read_val = 0;
};

TEST_F(TestChannel, test_channel_notification)
{
    Channel channel("chan", "float", 4);

    EXPECT_EQ(channel.array()->byte_size(), 4 * sizeof(float));
    EXPECT_EQ(channel.array()->data_size(), 4u);
    EXPECT_STREQ(channel.array()->data_type_str(), "float");
    EXPECT_EQ(channel.array()->capacity(), 4u);
    EXPECT_EQ(channel.array()->data_type(), TYPE_FLOAT);
    EXPECT_EQ(_notified[&channel], 0);
    channel.notify();
    EXPECT_EQ(_notified[&channel], 0);
    channel.add_observer(this);
    channel.notify();
    EXPECT_EQ(_notified[&channel], 1);
}

TEST_F(TestChannel, test_channel_read_write)
{
    Channel channel("chan", "int");

    channel.add_observer(this);
    EXPECT_EQ(_notified[&channel], 0);
    EXPECT_NO_THROW(EXPECT_EQ(channel.read<int>(0), 0));
    EXPECT_TRUE(channel.write<int>(0, 20));
    EXPECT_EQ(_notified[&channel], 1);
    EXPECT_NO_THROW(EXPECT_EQ(channel.read<int>(0), 20));

    EXPECT_EQ(_at_val, 20);
    EXPECT_EQ(_c_arr_val, 20);
    EXPECT_EQ(_read_val, 20);
}

TEST_F(TestChannel, test_channel_write_array)
{
    Channel channel("chan", "double", 6);

    channel.add_observer(this);
    ArrDouble arr = {1.0, 1.1, 1.2, 1.3, 1.4, 1.5};
    EXPECT_EQ(_notified[&channel], 0);
    channel.write(arr);
    EXPECT_EQ(_notified[&channel], 1);
    EXPECT_NO_THROW(
        { EXPECT_DOUBLE_EQ(channel.read<double>(0), 1.0); } { EXPECT_DOUBLE_EQ(channel.read<double>(1), 1.1); } {
            EXPECT_DOUBLE_EQ(channel.read<double>(2), 1.2);
        } { EXPECT_DOUBLE_EQ(channel.read<double>(3), 1.3); } { EXPECT_DOUBLE_EQ(channel.read<double>(4), 1.4); } {
            EXPECT_DOUBLE_EQ(channel.read<double>(5), 1.5);
        });
    channel.write(arr);
    EXPECT_EQ(_notified[&channel], 1);
    channel.set_write_on_change(false);
    channel.write(arr);
    EXPECT_EQ(_notified[&channel], 2);
}

TEST_F(TestChannel, test_channel_set_clock)
{
    FakeClock clock;
    Channel channel("chan", "int");

    channel.write<int>(0, 1);
    EXPECT_GT(channel.timestamp().get(), 0);

    channel.set_clock(&clock);
    EXPECT_TRUE(channel.write<int>(0, 2));
    EXPECT_EQ(channel.timestamp().get(), time::milli(1));
    EXPECT_TRUE(channel.write<int>(0, 3));
    EXPECT_EQ(channel.timestamp().get(), time::milli(2));
}

TEST_F(TestChannel, test_channel_clock_restore)
{
    FakeClock clock;
    Channel channel("chan", "int");

    channel.set_clock(&clock);
    EXPECT_TRUE(channel.write<int>(0, 1));
    EXPECT_EQ(channel.timestamp().get(), time::milli(1));

    channel.set_clock(Channel::default_clock());
    EXPECT_TRUE(channel.write<int>(0, 2));
    EXPECT_GT(channel.timestamp().get(), time::milli(1));
}

TEST_F(TestChannel, test_channel_write_from_channel)
{
    Channel dst("dst", "int", 2);
    Channel src("src", "int", 2);
    EXPECT_TRUE(src.write<int>(0, 11));
    EXPECT_TRUE(src.write<int>(1, 22));

    EXPECT_TRUE(dst.write(src));
    EXPECT_EQ(dst.read<int>(0), 11);
    EXPECT_EQ(dst.read<int>(1), 22);
}

TEST_F(TestChannel, test_channel_resize)
{
    Channel channel("chan", "int", 1);
    EXPECT_FALSE(channel.resizable());
    EXPECT_TRUE(channel.resize(4));
    EXPECT_EQ(channel.size(), 4u);
    EXPECT_TRUE(channel.write<int>(3, 9));
    EXPECT_EQ(channel.read<int>(3), 9);
}

TEST_F(TestChannel, test_channel_conf)
{
    EXPECT_EQ(Channel::build(""), nullptr);
    EXPECT_EQ(Channel::build("name=test"), nullptr);
    EXPECT_EQ(Channel::build("name=test;type=int"), nullptr);
    EXPECT_EQ(Channel::build("name=test;type=int;size=toto"), nullptr);
    EXPECT_THROW(Channel::build("name=test;type=inttt;size=2"), std::invalid_argument);
    Channel *channel = nullptr;
    EXPECT_NO_THROW(channel = Channel::build("name=chan;type=float;size=2"));
    ASSERT_NE(channel, nullptr);
    EXPECT_EQ(channel->name(), "chan");
    EXPECT_STREQ(channel->array()->data_type_str(), "float");
    EXPECT_EQ(channel->array()->size(), 2u);
    delete channel;
}

TEST_F(TestChannel, test_channel_copy_to)
{
    Channel channel("chan", "int", 5);
    ArrInt src = {10, 20, 30, 40, 50};
    channel.write(src);

    ArrInt dst;
    dst.resize(5);
    EXPECT_TRUE(channel.copy_to(dst));
    EXPECT_EQ(dst[0], 10);
    EXPECT_EQ(dst[1], 20);
    EXPECT_EQ(dst[2], 30);
    EXPECT_EQ(dst[3], 40);
    EXPECT_EQ(dst[4], 50);
}

TEST_F(TestChannel, test_channel_copy_to_slice)
{
    Channel channel("chan", "int", 5);
    ArrInt src = {10, 20, 30, 40, 50};
    channel.write(src);

    ArrInt dst;
    dst.resize(3);
    EXPECT_TRUE(channel.copy_to(dst, {1, 3}));
    EXPECT_EQ(dst[0], 20);
    EXPECT_EQ(dst[1], 30);
    EXPECT_EQ(dst[2], 40);
}

TEST_F(TestChannel, test_channel_copy_to_slice_negative)
{
    Channel channel("chan", "int", 5);
    ArrInt src = {10, 20, 30, 40, 50};
    channel.write(src);

    ArrInt dst;
    dst.resize(2);
    EXPECT_TRUE(channel.copy_to(dst, {-2, -1}));
    EXPECT_EQ(dst[0], 40);
    EXPECT_EQ(dst[1], 50);
}

TEST_F(TestChannel, test_channel_copy_to_bytes_slice)
{
    Channel channel("chan", "ubyte", 5);
    Array<uint8_t> src = {0x11, 0x22, 0x33, 0x44, 0x55};
    channel.write(src);

    Array<uint8_t> dst;
    dst.resize(3);
    EXPECT_TRUE(channel.copy_to_bytes(dst, {1, 3}));
    EXPECT_EQ(dst[0], 0x22);
    EXPECT_EQ(dst[1], 0x33);
    EXPECT_EQ(dst[2], 0x44);
}

TEST_F(TestChannel, test_channel_copy_to_slice_empty)
{
    Channel channel("chan", "int", 5);
    ArrInt src = {10, 20, 30, 40, 50};
    channel.write(src);

    ArrInt dst;
    dst.resize(1);
    EXPECT_FALSE(channel.copy_to(dst, {10, 3}));
    EXPECT_FALSE(channel.copy_to(dst, {3, 1}));
    EXPECT_FALSE(channel.copy_to_bytes(dst, {100, 50}));
}

TEST_F(TestChannel, test_channel_copy_to_slice_clamp)
{
    Channel channel("chan", "int", 3);
    ArrInt src = {10, 20, 30};
    channel.write(src);

    ArrInt dst;
    dst.resize(3);
    EXPECT_TRUE(channel.copy_to(dst, {0, 100}));
    EXPECT_EQ(dst[0], 10);
    EXPECT_EQ(dst[1], 20);
    EXPECT_EQ(dst[2], 30);
}

TEST_F(TestChannel, test_channel_copy_to_timestamp)
{
    Channel channel("chan", "int");
    ArrInt src = {42};
    channel.write(src);

    ArrInt dst;
    dst.resize(1);
    Timestamp ts(0);
    EXPECT_TRUE(channel.copy_to(dst, &ts));
    EXPECT_EQ(dst[0], 42);
    EXPECT_GT(ts, 0);
}

TEST_F(TestChannel, test_channel_resizable)
{
    Channel channel("chan", "byte", 4);
    channel.reserve(64);
    channel.set_resizable(true);

    EXPECT_TRUE(channel.resizable());
    EXPECT_EQ(channel.size(), 4u);
    EXPECT_EQ(channel.capacity(), 64u);

    ArrByte small = {1, 2, 3, 4};
    EXPECT_TRUE(channel.write(small));
    EXPECT_EQ(channel.size(), 4u);

    ArrByte bigger = {10, 20, 30, 40, 50, 60, 70, 80};
    EXPECT_TRUE(channel.write(bigger));
    EXPECT_EQ(channel.byte_size(), 8u);
    EXPECT_EQ(channel.read<int8_t>(7), 80);

    ArrByte too_big;
    too_big.resize(128);
    EXPECT_FALSE(channel.write(too_big));
}

TEST_F(TestChannel, test_channel_not_resizable)
{
    Channel channel("chan", "byte", 4);

    EXPECT_FALSE(channel.resizable());
    ArrByte bigger = {1, 2, 3, 4, 5};
    EXPECT_FALSE(channel.write(bigger));
}

TEST_F(TestChannel, test_channel_build_capacity)
{
    Channel *channel = Channel::build("name=rx;type=byte;size=16;capacity=1024");
    ASSERT_NE(channel, nullptr);
    EXPECT_TRUE(channel->resizable());
    EXPECT_EQ(channel->size(), 16u);
    EXPECT_EQ(channel->capacity(), 1024u);

    ArrByte data;
    data.resize(512);
    EXPECT_TRUE(channel->write(data));
    EXPECT_EQ(channel->byte_size(), 512u);

    delete channel;
}

} // namespace test

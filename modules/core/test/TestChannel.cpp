#include <gtest/gtest.h>

#include <sihd/util/Array.hpp>
#include <sihd/util/Logger.hpp>

#include <sihd/core/Channel.hpp>

namespace test
{
SIHD_NEW_LOGGER("sihd::test");
using namespace sihd::util;
using namespace sihd::core;
class TestChannel: public ::testing::Test,
                   public IHandler<Channel *>
{
    protected:
        TestChannel() { sihd::util::LoggerManager::stream(); }

        virtual ~TestChannel() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}

        void handle(Channel *c)
        {
            SIHD_TRACEV(c->array()->data_type_str());
            if (c->array()->data_type() == TYPE_INT)
            {
                const ArrInt *arr_int = dynamic_cast<const ArrInt *>(c->array());
                ASSERT_NE(arr_int, nullptr);
                const int *c_arr_int = arr_int->data();
                EXPECT_NO_THROW(_at_val = arr_int->at(0); _c_arr_val = c_arr_int[0];
                                _read_val = c->read<int>(0););
            }
            _notified[c] += 1;
        }

        std::map<Channel *, int> _notified;
        int _at_val = 0;
        int _c_arr_val = 0;
        int _read_val = 0;
};

TEST_F(TestChannel, test_channel_notification)
{
    Channel c("chan", "float", 4);

    EXPECT_EQ(c.array()->byte_size(), 4 * sizeof(float));
    EXPECT_EQ(c.array()->data_size(), 4u);
    EXPECT_STREQ(c.array()->data_type_str(), "float");
    EXPECT_EQ(c.array()->capacity(), 4u);
    EXPECT_EQ(c.array()->data_type(), TYPE_FLOAT);
    EXPECT_EQ(_notified[&c], 0);
    c.notify();
    EXPECT_EQ(_notified[&c], 0);
    c.add_observer(this);
    c.notify();
    EXPECT_EQ(_notified[&c], 1);
}

TEST_F(TestChannel, test_channel_read_write)
{
    Channel c("chan", "int");

    c.add_observer(this);
    EXPECT_EQ(_notified[&c], 0);
    EXPECT_NO_THROW(EXPECT_EQ(c.read<int>(0), 0));
    EXPECT_TRUE(c.write<int>(0, 20));
    EXPECT_EQ(_notified[&c], 1);
    EXPECT_NO_THROW(EXPECT_EQ(c.read<int>(0), 20));

    EXPECT_EQ(_at_val, 20);
    EXPECT_EQ(_c_arr_val, 20);
    EXPECT_EQ(_read_val, 20);
}

TEST_F(TestChannel, test_channel_write_array)
{
    Channel c("chan", "double", 6);

    c.add_observer(this);
    ArrDouble arr = {1.0, 1.1, 1.2, 1.3, 1.4, 1.5};
    EXPECT_EQ(_notified[&c], 0);
    c.write(arr);
    EXPECT_EQ(_notified[&c], 1);
    EXPECT_NO_THROW(
        { EXPECT_DOUBLE_EQ(c.read<double>(0), 1.0); } { EXPECT_DOUBLE_EQ(c.read<double>(1), 1.1); } {
            EXPECT_DOUBLE_EQ(c.read<double>(2), 1.2);
        } { EXPECT_DOUBLE_EQ(c.read<double>(3), 1.3); } { EXPECT_DOUBLE_EQ(c.read<double>(4), 1.4); } {
            EXPECT_DOUBLE_EQ(c.read<double>(5), 1.5);
        });
    c.write(arr);
    EXPECT_EQ(_notified[&c], 1);
    c.set_write_on_change(false);
    c.write(arr);
    EXPECT_EQ(_notified[&c], 2);
}

TEST_F(TestChannel, test_channel_conf)
{
    EXPECT_EQ(Channel::build(""), nullptr);
    EXPECT_EQ(Channel::build("name=test"), nullptr);
    EXPECT_EQ(Channel::build("name=test;type=int"), nullptr);
    EXPECT_EQ(Channel::build("name=test;type=int;size=toto"), nullptr);
    EXPECT_THROW(Channel::build("name=test;type=inttt;size=2"), std::invalid_argument);
    Channel *c = nullptr;
    EXPECT_NO_THROW(c = Channel::build("name=chan;type=float;size=2"));
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->name(), "chan");
    EXPECT_STREQ(c->array()->data_type_str(), "float");
    EXPECT_EQ(c->array()->size(), 2u);
    delete c;
}

TEST_F(TestChannel, test_channel_copy_to)
{
    Channel c("chan", "int", 5);
    ArrInt src = {10, 20, 30, 40, 50};
    c.write(src);

    ArrInt dst;
    dst.resize(5);
    EXPECT_TRUE(c.copy_to(dst));
    EXPECT_EQ(dst[0], 10);
    EXPECT_EQ(dst[1], 20);
    EXPECT_EQ(dst[2], 30);
    EXPECT_EQ(dst[3], 40);
    EXPECT_EQ(dst[4], 50);
}

TEST_F(TestChannel, test_channel_copy_to_slice)
{
    Channel c("chan", "int", 5);
    ArrInt src = {10, 20, 30, 40, 50};
    c.write(src);

    ArrInt dst;
    dst.resize(3);
    EXPECT_TRUE(c.copy_to(dst, {1, 3}));
    EXPECT_EQ(dst[0], 20);
    EXPECT_EQ(dst[1], 30);
    EXPECT_EQ(dst[2], 40);
}

TEST_F(TestChannel, test_channel_copy_to_slice_negative)
{
    Channel c("chan", "int", 5);
    ArrInt src = {10, 20, 30, 40, 50};
    c.write(src);

    ArrInt dst;
    dst.resize(2);
    EXPECT_TRUE(c.copy_to(dst, {-2, -1}));
    EXPECT_EQ(dst[0], 40);
    EXPECT_EQ(dst[1], 50);
}

TEST_F(TestChannel, test_channel_copy_to_bytes_slice)
{
    Channel c("chan", "ubyte", 5);
    Array<uint8_t> src = {0x11, 0x22, 0x33, 0x44, 0x55};
    c.write(src);

    Array<uint8_t> dst;
    dst.resize(3);
    EXPECT_TRUE(c.copy_to_bytes(dst, {1, 3}));
    EXPECT_EQ(dst[0], 0x22);
    EXPECT_EQ(dst[1], 0x33);
    EXPECT_EQ(dst[2], 0x44);
}

TEST_F(TestChannel, test_channel_copy_to_slice_empty)
{
    Channel c("chan", "int", 5);
    ArrInt src = {10, 20, 30, 40, 50};
    c.write(src);

    ArrInt dst;
    dst.resize(1);
    EXPECT_FALSE(c.copy_to(dst, {10, 3}));
    EXPECT_FALSE(c.copy_to(dst, {3, 1}));
    EXPECT_FALSE(c.copy_to_bytes(dst, {100, 50}));
}

TEST_F(TestChannel, test_channel_copy_to_slice_clamp)
{
    Channel c("chan", "int", 3);
    ArrInt src = {10, 20, 30};
    c.write(src);

    ArrInt dst;
    dst.resize(3);
    EXPECT_TRUE(c.copy_to(dst, {0, 100}));
    EXPECT_EQ(dst[0], 10);
    EXPECT_EQ(dst[1], 20);
    EXPECT_EQ(dst[2], 30);
}

TEST_F(TestChannel, test_channel_copy_to_timestamp)
{
    Channel c("chan", "int");
    ArrInt src = {42};
    c.write(src);

    ArrInt dst;
    dst.resize(1);
    Timestamp ts = 0;
    EXPECT_TRUE(c.copy_to(dst, &ts));
    EXPECT_EQ(dst[0], 42);
    EXPECT_GT(ts, 0);
}

TEST_F(TestChannel, test_channel_resizable)
{
    Channel c("chan", "byte", 4);
    c.reserve(64);
    c.set_resizable(true);

    EXPECT_TRUE(c.resizable());
    EXPECT_EQ(c.size(), 4u);
    EXPECT_EQ(c.capacity(), 64u);

    ArrByte small = {1, 2, 3, 4};
    EXPECT_TRUE(c.write(small));
    EXPECT_EQ(c.size(), 4u);

    ArrByte bigger = {10, 20, 30, 40, 50, 60, 70, 80};
    EXPECT_TRUE(c.write(bigger));
    EXPECT_EQ(c.byte_size(), 8u);
    EXPECT_EQ(c.read<int8_t>(7), 80);

    ArrByte too_big;
    too_big.resize(128);
    EXPECT_FALSE(c.write(too_big));
}

TEST_F(TestChannel, test_channel_not_resizable)
{
    Channel c("chan", "byte", 4);

    EXPECT_FALSE(c.resizable());
    ArrByte bigger = {1, 2, 3, 4, 5};
    EXPECT_FALSE(c.write(bigger));
}

TEST_F(TestChannel, test_channel_build_capacity)
{
    Channel *c = Channel::build("name=rx;type=byte;size=16;capacity=1024");
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->resizable());
    EXPECT_EQ(c->size(), 16u);
    EXPECT_EQ(c->capacity(), 1024u);

    ArrByte data;
    data.resize(512);
    EXPECT_TRUE(c->write(data));
    EXPECT_EQ(c->byte_size(), 512u);

    delete c;
}

} // namespace test

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
            SIHD_TRACEF(c->array()->data_type_str());
            if (c->array()->data_type() == TYPE_INT)
            {
                const ArrInt *arr_int = dynamic_cast<const ArrInt *>(c->array());
                ASSERT_NE(arr_int, nullptr);
                const int *c_arr_int = arr_int->data();
                EXPECT_NO_THROW(_at_val = arr_int->at(0); _c_arr_val = c_arr_int[0]; _read_val = c->read<int>(0););
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

} // namespace test

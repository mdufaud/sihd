#include <array>
#include <cstring>

#include <gtest/gtest.h>

#include <sihd/core/Core.hpp>
#include <sihd/core/DevMessage.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Message.hpp>
#include <sihd/util/ansi.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::core;
using namespace sihd::util;

namespace
{

struct s_msg
{
        bool active;
        int size;
        std::array<char, 20> str;
} __attribute__((packed));

} // namespace

class TestDevMessage: public ::testing::Test
{
    protected:
        TestDevMessage() { LoggerManager::stream(); }

        virtual ~TestDevMessage() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestDevMessage, test_devmessage)
{
    Core core;
    DevMessage *dev = core.add_child<DevMessage>("dev");
    Message *msg = core.add_child<Message>("msg");

    msg->add_field("active", TYPE_BOOL, 1);
    msg->add_field("size", TYPE_INT, 1);
    msg->add_field("str", TYPE_CHAR, 20);

    msg->finish();

    (void)dev->set_conf_str("message", "..msg");
    dev->set_trigger_mode(false);

    ASSERT_TRUE(core.init());
    ASSERT_TRUE(core.start());

    SIHD_COUT("{}", core.tree_desc_str());

    auto [ch_active_in,
          ch_size_in,
          ch_str_in,
          ch_active_out,
          ch_size_out,
          ch_str_out,
          ch_buffer_in,
          ch_buffer_out,
          ch_trigger] = dev->get_all_child<Channel>("active_in",
                                                    "size_in",
                                                    "str_in",
                                                    "active_out",
                                                    "size_out",
                                                    "str_out",
                                                    "buffer_in",
                                                    "buffer_out",
                                                    "trigger");

    ASSERT_EQ(ch_active_in->size(), 1u);
    ASSERT_EQ(ch_size_in->size(), 1u);
    ASSERT_EQ(ch_str_in->size(), 20u);
    ASSERT_EQ(ch_active_out->size(), 1u);
    ASSERT_EQ(ch_buffer_in->size(), 25u);
    ASSERT_EQ(ch_buffer_out->size(), 25u);
    ASSERT_EQ(ch_trigger->size(), 1u);

    EXPECT_EQ(ch_size_out->read<int>(0), 0);
    EXPECT_TRUE(ch_size_in->write(0, 300));
    EXPECT_EQ(ch_size_out->read<int>(0), 300);
    EXPECT_EQ(ch_buffer_out->read<int>(1), 300);
    EXPECT_GT(ch_size_out->timestamp(), Timestamp(0));

    s_msg msgtest;
    memset(&msgtest, 0, sizeof(msgtest));
    msgtest.active = true;
    msgtest.size = 20;
    strcpy(msgtest.str.data(), "hello world");

    EXPECT_TRUE(ch_buffer_in->write({&msgtest, sizeof(msgtest)}));
    EXPECT_EQ(ch_active_out->read<bool>(0), true);
    EXPECT_EQ(ch_size_out->read<int>(0), 20);
    EXPECT_STREQ((const char *)ch_str_out->data(), "hello world");

    EXPECT_EQ(ch_buffer_out->read<bool>(0), true);
    EXPECT_EQ(ch_buffer_out->read<int>(1), 20);
    EXPECT_EQ(ch_buffer_out->read<char>(5), 'h');

    /*
    ** Testing trigger mode
    */

    dev->set_trigger_mode(true);

    EXPECT_TRUE(ch_size_in->write(0, 30));
    EXPECT_EQ(ch_buffer_out->read<int>(1), 20);
    ch_trigger->notify();
    EXPECT_EQ(ch_buffer_out->read<int>(1), 30);

    memset(&msgtest, 0, sizeof(msgtest));
    msgtest.active = false;
    msgtest.size = 40;
    strcpy(msgtest.str.data(), "toto");

    EXPECT_TRUE(ch_buffer_in->write({&msgtest, sizeof(msgtest)}));

    EXPECT_EQ(ch_active_out->read<bool>(0), true);
    EXPECT_EQ(ch_size_out->read<int>(0), 30);
    EXPECT_STREQ((const char *)ch_str_out->data(), "hello world");

    ch_trigger->notify();

    EXPECT_EQ(ch_active_out->read<bool>(0), false);
    EXPECT_EQ(ch_size_out->read<int>(0), 40);
    EXPECT_STREQ((const char *)ch_str_out->data(), "toto");

    ASSERT_TRUE(core.stop());
}

TEST_F(TestDevMessage, test_devmessage_trigger_notifies_only_changed_fields)
{
    Core core;
    DevMessage *dev = core.add_child<DevMessage>("dev");
    Message *msg = core.add_child<Message>("msg");
    msg->add_field("size", TYPE_INT, 1);
    ASSERT_TRUE(msg->finish());
    (void)dev->set_conf_str("message", "..msg");
    dev->set_trigger_mode(true);
    ASSERT_TRUE(core.init());
    ASSERT_TRUE(core.start());

    auto [ch_size_in, ch_size_out, ch_buffer_out, ch_trigger] = dev->get_all_child<Channel>("size_in",
                                                                                            "size_out",
                                                                                            "buffer_out",
                                                                                            "trigger");
    int size_notified = 0;
    int buffer_notified = 0;
    Handler<Channel *> size_observer([&size_notified](Channel *) { ++size_notified; });
    Handler<Channel *> buffer_observer([&buffer_notified](Channel *) { ++buffer_notified; });
    ch_size_out->add_observer(&size_observer);
    ch_buffer_out->add_observer(&buffer_observer);

    EXPECT_TRUE(ch_size_in->write(0, 30));
    ch_trigger->notify();
    ch_trigger->notify();

    EXPECT_EQ(size_notified, 1);
    EXPECT_EQ(buffer_notified, 3);
    EXPECT_EQ(ch_buffer_out->read<int>(0), 30);
    ASSERT_TRUE(core.stop());
}

TEST_F(TestDevMessage, test_devmessage_reset_then_other_message)
{
    Core core;
    DevMessage *dev = core.add_child<DevMessage>("dev");
    Message *first = core.add_child<Message>("first");
    first->add_field("a", TYPE_INT, 1);
    ASSERT_TRUE(first->finish());
    Message *second = core.add_child<Message>("second");
    second->add_field("b", TYPE_INT, 1);
    second->add_field("c", TYPE_INT, 1);
    ASSERT_TRUE(second->finish());

    dev->set_trigger_mode(false);
    (void)dev->set_conf_str("message", "..first");
    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());
    ASSERT_TRUE(dev->stop());
    ASSERT_TRUE(dev->reset());

    (void)dev->set_conf_str("message", "..second");
    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());
    auto [ch_buffer_in, ch_b_out, ch_c_out] = dev->get_all_child<Channel>("buffer_in", "b_out", "c_out");
    const std::array<int, 2> raw = {7, 8};

    EXPECT_TRUE(ch_buffer_in->write(ArrByteView((const uint8_t *)raw.data(), sizeof(raw))));

    EXPECT_EQ(ch_b_out->read<int>(0), 7);
    EXPECT_EQ(ch_c_out->read<int>(0), 8);
    ASSERT_TRUE(dev->stop());
}

TEST_F(TestDevMessage, test_devmessage_init_unknown_message)
{
    Core core;
    DevMessage *dev = core.add_child<DevMessage>("dev");
    (void)dev->set_conf_str("message", "..no_such_message");
    EXPECT_FALSE(core.init());
}

TEST_F(TestDevMessage, test_devmessage_init_unfinished)
{
    Core core;
    DevMessage *dev = core.add_child<DevMessage>("dev");
    Message *msg = core.add_child<Message>("msg");
    msg->add_field("size", TYPE_INT, 1);
    (void)dev->set_conf_str("message", "..msg");

    EXPECT_FALSE(core.init());
    EXPECT_FALSE(core.reset());
    EXPECT_FALSE(core.init());
    (void)msg->finish();
    EXPECT_FALSE(core.init());
}
} // namespace test

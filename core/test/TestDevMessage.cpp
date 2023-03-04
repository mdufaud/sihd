#include <gtest/gtest.h>
#include <sihd/core/Core.hpp>
#include <sihd/core/DevMessage.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Message.hpp>
#include <sihd/util/TmpDir.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/term.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::core;
using namespace sihd::util;
class TestDevMessage: public ::testing::Test
{
    protected:
        TestDevMessage() { sihd::util::LoggerManager::basic(); }

        virtual ~TestDevMessage() { sihd::util::LoggerManager::clear_loggers(); }

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

    struct s_msg
    {
            bool active;
            int size;
            char str[20];
    } __attribute__((packed));

    dev->set_conf_str("message", "..msg");
    // immediate compute mode
    dev->set_trigger_mode(false);

    ASSERT_TRUE(core.init());
    ASSERT_TRUE(core.start());

    SIHD_COUT(core.tree_desc_str());

    auto [ch_active_in,
          ch_size_in,
          ch_str_in,
          ch_active_out,
          ch_size_out,
          ch_str_out,
          ch_buffer_in,
          ch_buffer_out,
          ch_trigger]
        = dev->get_all_child<Channel>("active_in",
                                      "size_in",
                                      "str_in",
                                      "active_out",
                                      "size_out",
                                      "str_out",
                                      "buffer_in",
                                      "buffer_out",
                                      "trigger");

    ASSERT_EQ(ch_active_in->size(), 1);
    ASSERT_EQ(ch_size_in->size(), 1);
    ASSERT_EQ(ch_str_in->size(), 20);
    ASSERT_EQ(ch_active_out->size(), 1);
    ASSERT_EQ(ch_buffer_in->size(), 25);
    ASSERT_EQ(ch_buffer_out->size(), 25);
    ASSERT_EQ(ch_trigger->size(), 1);

    EXPECT_EQ(ch_size_out->read<int>(0), 0);
    EXPECT_TRUE(ch_size_in->write(0, 10));
    EXPECT_EQ(ch_size_out->read<int>(0), 10);
    EXPECT_EQ(ch_buffer_out->read<int>(1), 10);

    s_msg msgtest;
    memset(&msgtest, 0, sizeof(msgtest));
    msgtest.active = true;
    msgtest.size = 20;
    strcpy(msgtest.str, "hello world");

    EXPECT_TRUE(ch_buffer_in->write({&msgtest, sizeof(msgtest)}));
    EXPECT_EQ(ch_active_out->read<bool>(0), true);
    EXPECT_EQ(ch_size_out->read<int>(0), 20);
    EXPECT_STREQ((char *)ch_str_out->data(), "hello world");

    EXPECT_EQ(ch_buffer_out->read<bool>(0), true);
    EXPECT_EQ(ch_buffer_out->read<int>(1), 20);
    EXPECT_EQ(ch_buffer_out->read<char>(5), 'h');

    /*
    ** Testing trigger mode
    */

    dev->set_trigger_mode(true);

    EXPECT_TRUE(ch_size_in->write(0, 30));
    // still old value
    EXPECT_EQ(ch_buffer_out->read<int>(1), 20);
    ch_trigger->notify();
    // new value after trigger
    EXPECT_EQ(ch_buffer_out->read<int>(1), 30);

    memset(&msgtest, 0, sizeof(msgtest));
    msgtest.active = false;
    msgtest.size = 40;
    strcpy(msgtest.str, "toto");

    EXPECT_TRUE(ch_buffer_in->write({&msgtest, sizeof(msgtest)}));

    // still old values
    EXPECT_EQ(ch_active_out->read<bool>(0), true);
    EXPECT_EQ(ch_size_out->read<int>(0), 30);
    EXPECT_STREQ((char *)ch_str_out->data(), "hello world");

    ch_trigger->notify();

    // new values after trigger
    EXPECT_EQ(ch_active_out->read<bool>(0), false);
    EXPECT_EQ(ch_size_out->read<int>(0), 40);
    EXPECT_STREQ((char *)ch_str_out->data(), "toto");

    ASSERT_TRUE(core.stop());
}
} // namespace test

#include <gtest/gtest.h>
#include <sihd/core/Device.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::core;
class TestDevice: public ::testing::Test
{
    protected:
        TestDevice() { sihd::util::LoggerManager::stream(); }

        virtual ~TestDevice() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

class SomeDevice: public Device
{
    public:
        SomeDevice(const std::string & name, Node *parent = nullptr): Device(name, parent) { _running = false; }

        ~SomeDevice() {}

        bool is_running() const { return _running; }

        void handle(Channel *c) { (void)c; }

    protected:
        bool on_init()
        {
            // returns nullptr if channel is linked
            this->add_channel("c1", "byte");
            this->add_unlinked_channel("c2", "int");
            return true;
        }

        bool on_start()
        {
            _running = true;
            return true;
        }

        bool on_stop()
        {
            _running = false;
            return true;
        }

    private:
        bool _running;
};

TEST_F(TestDevice, test_device_service)
{
    Node root("root");
    new Channel("declared_channel", "int", &root);
    new Channel("extra_channel", "double", &root);

    SomeDevice *dev = root.add_child<SomeDevice>("device");
    dev->add_link("c2", "..declared_channel");
    dev->add_link("c3", "..extra_channel");

    EXPECT_EQ(dev->get_channel("c1"), nullptr);
    EXPECT_EQ(dev->get_channel("c2"), nullptr);
    EXPECT_EQ(dev->get_channel("c3"), nullptr);
    // device init the channel
    EXPECT_TRUE(dev->init());
    EXPECT_NE(dev->get_channel("c1"), nullptr);
    EXPECT_EQ(dev->get_channel("c2"), nullptr);
    EXPECT_EQ(dev->get_channel("c3"), nullptr);
    // links are done
    EXPECT_TRUE(dev->start());
    EXPECT_NE(dev->get_channel("c1"), nullptr);
    EXPECT_NE(dev->get_channel("c2"), nullptr);
    EXPECT_NE(dev->get_channel("c3"), nullptr);
    // remove observations
    EXPECT_TRUE(dev->stop());
    EXPECT_NE(dev->get_channel("c1"), nullptr);
    EXPECT_NE(dev->get_channel("c2"), nullptr);
    EXPECT_NE(dev->get_channel("c3"), nullptr);
    // delete_children
    EXPECT_TRUE(dev->reset());
    EXPECT_EQ(dev->get_channel("c1"), nullptr);
    EXPECT_EQ(dev->get_channel("c2"), nullptr);
    EXPECT_EQ(dev->get_channel("c3"), nullptr);
}
} // namespace test

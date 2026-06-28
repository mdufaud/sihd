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
        SomeDevice(const std::string & name, Node *parent = nullptr): Device(name, parent)
        {
            _running = false;
        }

        ~SomeDevice() = default;

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

class ResizableDevice: public Device
{
    public:
        ResizableDevice(const std::string & name, Node *parent = nullptr): Device(name, parent) {}

        ~ResizableDevice() = default;

        bool is_running() const { return false; }

        void handle(Channel *c) { (void)c; }

    protected:
        bool on_init()
        {
            this->add_channel_resizable("local", "byte", 1, 128);
            this->add_unlinked_channel_resizable("shared", "byte", 1, 128);
            return true;
        }

        bool on_start() { return true; }

        bool on_stop() { return true; }
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

TEST_F(TestDevice, test_device_channel_resizable)
{
    Node root("root");
    ResizableDevice *dev = root.add_child<ResizableDevice>("device");

    EXPECT_TRUE(dev->init());
    Channel *c = dev->get_channel("local");
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->resizable());
    EXPECT_EQ(c->size(), 1u);
    EXPECT_EQ(c->capacity(), 128u);
}

TEST_F(TestDevice, test_device_resizable_link_match)
{
    Node root("root");
    Channel *ext = new Channel("ext", "byte", 1, &root);
    ext->reserve(256);
    ext->set_resizable(true);

    ResizableDevice *dev = root.add_child<ResizableDevice>("device");
    dev->add_link("shared", "..ext");

    EXPECT_TRUE(dev->init());
    // both ends resizable -> link contract satisfied
    EXPECT_TRUE(dev->start());
    EXPECT_TRUE(dev->stop());
}

TEST_F(TestDevice, test_device_resizable_link_mismatch)
{
    Node root("root");
    // ext is not resizable -> contract violated
    new Channel("ext", "byte", 1, &root);

    ResizableDevice *dev = root.add_child<ResizableDevice>("device");
    dev->add_link("shared", "..ext");

    EXPECT_TRUE(dev->init());
    EXPECT_FALSE(dev->start());
}
} // namespace test

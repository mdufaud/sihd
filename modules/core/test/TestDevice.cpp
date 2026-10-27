#include <atomic>

#include <gtest/gtest.h>

#include <sihd/core/Channel.hpp>
#include <sihd/core/Core.hpp>
#include <sihd/core/Device.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/build.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::core;
class TestDevice: public ::testing::Test
{
    protected:
        TestDevice() { LoggerManager::stream(); }

        virtual ~TestDevice() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

class SomeDevice: public Device
{
    public:
        SomeDevice(const std::string & name, Node *parent = nullptr): Device(name, parent) { _running = false; }

        ~SomeDevice() = default;

        bool is_running() const { return _running; }

        void handle(Channel *channel) { (void)channel; }

    protected:
        bool on_init()
        {
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

        void handle(Channel *channel) { (void)channel; }

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

    EXPECT_EQ(dev->get_channel("c1").value_or(nullptr), nullptr);
    EXPECT_EQ(dev->get_channel("c2").value_or(nullptr), nullptr);
    EXPECT_EQ(dev->get_channel("c3").value_or(nullptr), nullptr);

    EXPECT_TRUE(dev->init());
    EXPECT_NE(dev->get_channel("c1").value_or(nullptr), nullptr);
    EXPECT_EQ(dev->get_channel("c2").value_or(nullptr), nullptr);
    EXPECT_EQ(dev->get_channel("c3").value_or(nullptr), nullptr);

    EXPECT_TRUE(dev->start());
    EXPECT_NE(dev->get_channel("c1").value_or(nullptr), nullptr);
    EXPECT_NE(dev->get_channel("c2").value_or(nullptr), nullptr);
    EXPECT_NE(dev->get_channel("c3").value_or(nullptr), nullptr);

    EXPECT_TRUE(dev->stop());
    EXPECT_NE(dev->get_channel("c1").value_or(nullptr), nullptr);
    EXPECT_NE(dev->get_channel("c2").value_or(nullptr), nullptr);
    EXPECT_NE(dev->get_channel("c3").value_or(nullptr), nullptr);

    EXPECT_TRUE(dev->reset());
    EXPECT_EQ(dev->get_channel("c1").value_or(nullptr), nullptr);
    EXPECT_EQ(dev->get_channel("c2").value_or(nullptr), nullptr);
    EXPECT_EQ(dev->get_channel("c3").value_or(nullptr), nullptr);
}

TEST_F(TestDevice, test_device_channel_resizable)
{
    Node root("root");
    ResizableDevice *dev = root.add_child<ResizableDevice>("device");

    EXPECT_TRUE(dev->init());
    Channel *channel = dev->get_channel("local").value_or(nullptr);
    ASSERT_NE(channel, nullptr);
    EXPECT_TRUE(channel->resizable());
    EXPECT_EQ(channel->size(), 1u);
    EXPECT_EQ(channel->capacity(), 128u);
}

TEST_F(TestDevice, test_device_resizable_link_match)
{
    Node root("root");
    Channel *ext = new Channel("ext", "byte", 1, &root);
    ext->reserve(256);
    ext->set_resizable(true);
    ASSERT_TRUE(ext->write(ArrByte {1, 2, 3}));

    ResizableDevice *dev = root.add_child<ResizableDevice>("device");
    dev->add_link("shared", "..ext");

    EXPECT_TRUE(dev->init());
    EXPECT_TRUE(dev->start());
    EXPECT_TRUE(dev->stop());
}

TEST_F(TestDevice, test_device_resizable_link_mismatch)
{
    Node root("root");
    new Channel("ext", "byte", 1, &root);
    Channel *small = new Channel("small", "byte", 1, &root);
    small->reserve(64);
    small->set_resizable(true);

    ResizableDevice *dev = root.add_child<ResizableDevice>("device");
    dev->add_link("shared", "..ext");
    ResizableDevice *small_dev = root.add_child<ResizableDevice>("small_device");
    small_dev->add_link("shared", "..small");

    EXPECT_TRUE(dev->init());
    EXPECT_FALSE(dev->start());
    EXPECT_TRUE(small_dev->init());
    EXPECT_FALSE(small_dev->start());
}

class FailingInitDevice: public Device
{
    public:
        FailingInitDevice(const std::string & name, Node *parent = nullptr): Device(name, parent) {}

        bool is_running() const override { return _running; }

        void set_fail(bool active) { _fail.store(active); }

    protected:
        bool on_init() override { return _fail.load() == false; }

        bool on_start() override
        {
            _running = true;
            return true;
        }

        bool on_stop() override
        {
            _running = false;
            return true;
        }

    private:
        std::atomic<bool> _fail = true;
        bool _running = false;
};

TEST_F(TestDevice, test_core_init_cascade_failure_is_terminal)
{
    Core core;
    FailingInitDevice *bad = core.add_child<FailingInitDevice>("bad");

    EXPECT_FALSE(core.init());

    bad->set_fail(false);
    EXPECT_FALSE(core.init());
    EXPECT_FALSE(core.start());
    EXPECT_FALSE(core.stop());
}

TEST_F(TestDevice, test_device_state_transitions)
{
    Node root("root");
    SomeDevice *dev = root.add_child<SomeDevice>("device");

    EXPECT_EQ(dev->device_state(), ServiceController::None);
    EXPECT_STREQ(dev->device_state_str(), "none");

    EXPECT_TRUE(dev->init());
    EXPECT_EQ(dev->device_state(), ServiceController::Stopped);
    EXPECT_STREQ(dev->device_state_str(), "stopped");

    EXPECT_TRUE(dev->start());
    EXPECT_EQ(dev->device_state(), ServiceController::Running);
    EXPECT_STREQ(dev->device_state_str(), "running");

    EXPECT_TRUE(dev->stop());
    EXPECT_EQ(dev->device_state(), ServiceController::Stopped);
    EXPECT_STREQ(dev->device_state_str(), "stopped");

    EXPECT_TRUE(dev->reset());
    EXPECT_EQ(dev->device_state(), ServiceController::None);
}

TEST_F(TestDevice, test_device_duplicate_channel_refused)
{
    Node root("root");
    SomeDevice *dev = root.add_child<SomeDevice>("device");

    Channel *first = dev->add_channel("dup", "int");
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(dev->add_channel("dup", "int"), nullptr);
    EXPECT_EQ(dev->get_channel("dup").value_or(nullptr), first);
}

TEST_F(TestDevice, test_device_link_type_mismatch)
{
    Node root("root");
    new Channel("dbl", "double", &root);

    SomeDevice *dev = root.add_child<SomeDevice>("device");
    EXPECT_TRUE(dev->add_link("c2", "..dbl"));
    EXPECT_TRUE(dev->init());
    EXPECT_FALSE(dev->start());
}

class BypassMatchDevice: public Device
{
    public:
        BypassMatchDevice(const std::string & name, Node *parent = nullptr): Device(name, parent) {}

        bool is_running() const override { return true; }

    protected:
        bool on_init() override
        {
            this->add_unlinked_channel("c2", "int", 1, false);
            return true;
        }

        bool on_start() override { return true; }
};

TEST_F(TestDevice, test_device_link_type_mismatch_bypassed)
{
    Node root("root");
    new Channel("dbl", "double", &root);

    BypassMatchDevice *dev = root.add_child<BypassMatchDevice>("device");
    EXPECT_TRUE(dev->add_link("c2", "..dbl"));
    EXPECT_TRUE(dev->init());
    EXPECT_TRUE(dev->start());
    Channel *c2 = dev->get_channel("c2").value_or(nullptr);
    ASSERT_NE(c2, nullptr);
    EXPECT_STREQ(c2->array()->data_type_str(), "double");
}

TEST_F(TestDevice, test_factories_registered)
{
    if constexpr (!SIHD_FACTORY_TABLE)
        GTEST_SKIP() << "factories resolve by symbol through sys, covered by sys TestPluginLoader";

    for (const auto & [class_name, full_name] : {std::pair {"Core", "sihd::core::Core"},
                                                 {"DevBatcher", "sihd::core::DevBatcher"},
                                                 {"DevFilter", "sihd::core::DevFilter"},
                                                 {"DevMessage", "sihd::core::DevMessage"},
                                                 {"DevPlayer", "sihd::core::DevPlayer"},
                                                 {"DevPulsation", "sihd::core::DevPulsation"},
                                                 {"DevRecorder", "sihd::core::DevRecorder"},
                                                 {"DevSampler", "sihd::core::DevSampler"},
                                                 {"DevStats", "sihd::core::DevStats"},
                                                 {"DevWorker", "sihd::core::DevWorker"},
                                                 {"DevWorkerPool", "sihd::core::DevWorkerPool"},
                                                 {"MemRecorder", "sihd::core::MemRecorder"}})
    {
        Named *named = create_from_factory_table(class_name, class_name, nullptr);
        ASSERT_NE(named, nullptr) << class_name;
        EXPECT_EQ(named->class_name(), full_name);
        delete named;
    }
}
} // namespace test

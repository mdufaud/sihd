#include <atomic>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/core/Channel.hpp>
#include <sihd/core/Core.hpp>
#include <sihd/core/DevWorker.hpp>
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

namespace
{

class FailingStartDevice: public Device
{
    public:
        FailingStartDevice(const std::string & name, Node *parent = nullptr): Device(name, parent) {}

        bool is_running() const override { return false; }

    protected:
        bool on_start() override { return false; }
};

class WarningLogger: public ALogger
{
    public:
        WarningLogger(): ALogger("warnings") {}

        void log(const LogInfo & info, std::string_view msg) override
        {
            if (info.level == LogLevel::warning)
                warnings.emplace_back(msg);
        }

        std::vector<std::string> warnings;
};

class JournalDevice: public Device
{
    public:
        JournalDevice(const std::string & name, Node *parent = nullptr): Device(name, parent) {}

        bool is_running() const override { return _running; }

        void set_journal(std::vector<std::string> *journal) { _journal = journal; }

    protected:
        bool on_start() override
        {
            _journal->emplace_back("start " + this->name());
            _running = true;
            return true;
        }

        bool on_stop() override
        {
            _journal->emplace_back("stop " + this->name());
            _running = false;
            return true;
        }

        bool on_reset() override
        {
            _journal->emplace_back("reset " + this->name());
            return true;
        }

    private:
        std::vector<std::string> *_journal = nullptr;
        bool _running = false;
};

class CountingDevice: public Device
{
    public:
        CountingDevice(const std::string & name, Node *parent = nullptr): Device(name, parent) {}

        bool is_running() const override { return _running; }

        size_t count() const { return _count; }

    protected:
        void handle(Channel *) override { ++_count; }

        bool on_init() override
        {
            this->add_unlinked_channel("in", "int");
            return true;
        }

        bool on_start() override
        {
            _running = this->observe_channel("in").has_value();
            return _running;
        }

        bool on_stop() override
        {
            _running = false;
            return true;
        }

    private:
        std::atomic<size_t> _count = 0;
        bool _running = false;
};

class PortDevice: public Device
{
    public:
        PortDevice(const std::string & name, Node *parent = nullptr): Device(name, parent) {}

        bool is_running() const override { return false; }

    protected:
        bool on_init() override
        {
            this->add_unlinked_channel("in", TYPE_INT, 1);
            return this->add_channel("out", TYPE_INT, 1) != nullptr;
        }
};

} // namespace

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

TEST_F(TestDevice, test_core_destroyed_after_a_failed_start_warns_nothing)
{
    WarningLogger logger;
    ASSERT_TRUE(LoggerManager::add(&logger));
    {
        Core core;
        core.add_child<SomeDevice>("started");
        core.add_child<FailingStartDevice>("failing");
        ASSERT_TRUE(core.init());

        EXPECT_FALSE(core.start());
        EXPECT_EQ(core.device_state(), ServiceController::StartFailure);
    }
    LoggerManager::rm(&logger);

    EXPECT_EQ(logger.warnings, std::vector<std::string> {});
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

TEST_F(TestDevice, test_device_stops_and_resets_in_start_order)
{
    std::vector<std::string> journal;
    Core core;
    for (const char *name : {"first", "second", "third"})
        core.add_child<JournalDevice>(name)->set_journal(&journal);

    ASSERT_TRUE(core.init());
    ASSERT_TRUE(core.start());
    EXPECT_TRUE(core.stop());
    EXPECT_TRUE(core.reset());

    const std::vector<std::string> expected = {"start first",
                                               "start second",
                                               "start third",
                                               "stop first",
                                               "stop second",
                                               "stop third",
                                               "reset first",
                                               "reset second",
                                               "reset third"};
    EXPECT_EQ(journal, expected);
}

TEST_F(TestDevice, test_device_root_destroyed_while_running_stops_and_resets_children)
{
    std::vector<std::string> journal;
    {
        SomeDevice root("root");
        for (const char *name : {"first", "second"})
            root.add_child<JournalDevice>(name)->set_journal(&journal);
        ASSERT_TRUE(root.init());
        ASSERT_TRUE(root.start());
    }

    const std::vector<std::string> expected =
        {"start first", "start second", "stop first", "stop second", "reset first", "reset second"};
    EXPECT_EQ(journal, expected);
}

TEST_F(TestDevice, test_device_stop_drains_a_worker_into_running_consumers)
{
    constexpr int writes = 1000;
    Core core;
    Channel *source = core.add_channel("source", "int");
    DevWorker *worker = core.add_child<DevWorker>("worker");
    CountingDevice *consumer = core.add_child<CountingDevice>("consumer");
    ASSERT_TRUE(worker->add_link("source", "..source"));
    ASSERT_TRUE(consumer->add_link("in", "..worker.source_out"));
    ASSERT_TRUE(core.init());
    ASSERT_TRUE(core.start());

    for (int i = 1; i <= writes; ++i)
        ASSERT_TRUE(source->write<int>(0, i));
    ASSERT_TRUE(core.stop());

    EXPECT_EQ(consumer->count(), static_cast<size_t>(writes));
}

TEST_F(TestDevice, test_device_reset_before_the_outputs_it_links)
{
    Core core;
    Channel *source = core.add_channel("source", "int");
    DevWorker *worker = core.add_child<DevWorker>("worker");
    SomeDevice *consumer = core.add_child<SomeDevice>("consumer");
    ASSERT_TRUE(worker->add_link("source", "..source"));
    ASSERT_TRUE(consumer->add_link("c2", "..worker.source_out"));

    ASSERT_TRUE(core.init());
    ASSERT_TRUE(core.start());
    ASSERT_TRUE(source->write<int>(0, 4));
    ASSERT_TRUE(core.stop());

    EXPECT_TRUE(core.reset());
}

TEST_F(TestDevice, test_device_reset_with_a_consumer_before_its_producer)
{
    Core core;
    PortDevice *consumer = core.add_child<PortDevice>("consumer");
    core.add_child<PortDevice>("producer");
    ASSERT_TRUE(consumer->add_link("in", "..producer.out"));

    ASSERT_TRUE(core.init());
    ASSERT_TRUE(core.start());
    ASSERT_TRUE(core.stop());

    EXPECT_TRUE(core.reset());
}

TEST_F(TestDevice, test_device_reset_with_mutual_links)
{
    Core core;
    PortDevice *first = core.add_child<PortDevice>("first");
    PortDevice *second = core.add_child<PortDevice>("second");
    ASSERT_TRUE(first->add_link("in", "..second.out"));
    ASSERT_TRUE(second->add_link("in", "..first.out"));

    ASSERT_TRUE(core.init());
    ASSERT_TRUE(core.start());
    ASSERT_TRUE(core.stop());

    EXPECT_TRUE(core.reset());
}

TEST_F(TestDevice, test_device_reset_with_a_chain_of_links)
{
    Core core;
    PortDevice *first = core.add_child<PortDevice>("first");
    PortDevice *second = core.add_child<PortDevice>("second");
    core.add_child<PortDevice>("third");
    ASSERT_TRUE(first->add_link("in", "..second.in"));
    ASSERT_TRUE(second->add_link("in", "..third.out"));

    ASSERT_TRUE(core.init());
    ASSERT_TRUE(core.start());
    ASSERT_TRUE(core.stop());

    EXPECT_TRUE(core.reset());
}

TEST_F(TestDevice, test_device_reset_refused_before_stop)
{
    Core core;
    PortDevice *port = core.add_child<PortDevice>("port");
    ASSERT_TRUE(core.init());
    ASSERT_TRUE(core.start());

    EXPECT_FALSE(core.reset());
    EXPECT_FALSE(port->reset());
    EXPECT_TRUE(port->get_channel("out").has_value());

    ASSERT_TRUE(core.stop());
    EXPECT_TRUE(core.reset());
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

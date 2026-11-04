#include <atomic>
#include <chrono>
#include <clock_helper.hpp>
#include <functional>
#include <thread>

#include <fmt/std.h>
#include <gtest/gtest.h>

#include <sihd/core/Channel.hpp>
#include <sihd/core/Device.hpp>
#include <sihd/core/TreeMonitor.hpp>
#include <sihd/core/TreeProfiler.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/LogInfo.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Synchronizer.hpp>
#include <sihd/util/time.hpp>

#include "tree_helper.hpp"

namespace test
{
using namespace sihd::util;
using namespace sihd::core;

class LambdaObserver: public Named,
                      public IHandler<Channel *>
{
    public:
        LambdaObserver(const std::string & name, std::function<void(Channel *)> fun): Named(name), _fun(std::move(fun))
        {
        }

        void handle(Channel *c) override { _fun(c); }

    private:
        std::function<void(Channel *)> _fun;
};

// reports from its own op body, where nodes may die before the op ends
class ReportingDevice: public Device
{
    public:
        ReportingDevice(const std::string & name, Node *parent = nullptr): Device(name, parent) {}

        bool is_running() const override { return _running; }

        void set_profiler(TreeProfiler *profiler) { _profiler = profiler; }

        const std::string & report() const { return _report; }

    protected:
        bool on_start() override
        {
            if (_profiler != nullptr)
                _report = _profiler->report_str();
            _running = true;
            return true;
        }

    private:
        TreeProfiler *_profiler = nullptr;
        std::string _report;
        bool _running = false;
};

class LinkingDevice: public Device
{
    public:
        LinkingDevice(const std::string & name, Node *parent = nullptr): Device(name, parent) {}

        bool is_running() const override { return _running; }

        void handle([[maybe_unused]] Channel *c) override { ++handled; }

        int handled = 0;

    protected:
        bool on_init() override
        {
            this->add_channel("c1", "int");
            this->add_unlinked_channel("c2", "int");
            return true;
        }

        bool on_start() override
        {
            (void)this->observe_channel("c1");
            (void)this->observe_channel("c2");
            _running = true;
            return true;
        }

        bool on_stop() override
        {
            _running = false;
            return true;
        }

    private:
        bool _running = false;
};

class EventCollector: public IHandler<TreeProfiler::Event *>
{
    public:
        void handle(TreeProfiler::Event *event) override
        {
            EXPECT_GT(event->id, last_id);
            last_id = event->id;
            whats.push_back(event->what);
            sources.push_back(event->source);
            kinds.push_back(event->kind);
            ++calls;
        }

        size_t last_id = 0;
        int calls = 0;
        std::vector<std::string> whats;
        std::vector<std::string> sources;
        std::vector<TreeProfiler::Event::Kind> kinds;
};

class TestTreeProfiler: public ::testing::Test
{
    protected:
        TestTreeProfiler() { LoggerManager::stream(); }

        virtual ~TestTreeProfiler() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestTreeProfiler, test_observe_tree)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");
    new Channel("plain", "int", &root);

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());

    Channel *c1 = dev->get_channel("c1").value_or(nullptr);
    Channel *c2 = dev->get_channel("c2").value_or(nullptr);
    ASSERT_NE(c1, nullptr);
    ASSERT_NE(c2, nullptr);

    NamedObserver extra("extra");
    c1->add_observer(&extra);

    FakeClock clock;
    TreeMonitor monitor;
    TreeProfiler profiler(monitor);
    monitor.set_clock(&clock);
    ASSERT_TRUE(monitor.observe(&root));

    EXPECT_EQ(monitor.channels_count(), 3u);
    EXPECT_EQ(monitor.services_count(), 1u);

    EXPECT_TRUE(c1->write<int>(0, 1));
    EXPECT_TRUE(c1->write<int>(0, 2));
    EXPECT_TRUE(c1->write<int>(0, 3));
    EXPECT_TRUE(c2->write<int>(0, 1));

    EXPECT_EQ(dev->handled, 4);
    EXPECT_EQ(extra.calls, 3);

    const std::string expected = R"(root: sihd::util::Node
  device: test::ProfilingDevice [running]
    c1: sihd::core::Channel
      writes=3 total=+15ms:0us avg=+5ms:0us min=+5ms:0us max=+5ms:0us
      -> device (test::ProfilingDevice): n=3 total=+3ms:0us avg=+1ms:0us min=+1ms:0us max=+1ms:0us
      -> extra (test::NamedObserver): n=3 total=+3ms:0us avg=+1ms:0us min=+1ms:0us max=+1ms:0us
    c2: sihd::core::Channel
      writes=1 total=+3ms:0us avg=+3ms:0us min=+3ms:0us max=+3ms:0us
      -> device (test::ProfilingDevice): n=1 total=+1ms:0us avg=+1ms:0us min=+1ms:0us max=+1ms:0us
  plain: sihd::core::Channel
)";
    EXPECT_EQ(profiler.report_str(), expected);
}

TEST_F(TestTreeProfiler, test_observe_late_observer)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());

    FakeClock clock;
    TreeMonitor monitor;
    TreeProfiler profiler(monitor);
    monitor.set_clock(&clock);
    ASSERT_TRUE(monitor.observe(&root));

    NamedObserver late("late");
    Channel *c1 = dev->get_channel("c1").value_or(nullptr);
    ASSERT_NE(c1, nullptr);
    c1->add_observer(&late);

    EXPECT_TRUE(c1->write<int>(0, 1));
    EXPECT_EQ(late.calls, 1);

    const std::string expected = R"(root: sihd::util::Node
  device: test::ProfilingDevice [running]
    c1: sihd::core::Channel
      writes=1 total=+5ms:0us avg=+5ms:0us min=+5ms:0us max=+5ms:0us
      -> device (test::ProfilingDevice): n=1 total=+1ms:0us avg=+1ms:0us min=+1ms:0us max=+1ms:0us
      -> late (test::NamedObserver): n=1 total=+1ms:0us avg=+1ms:0us min=+1ms:0us max=+1ms:0us
    c2: sihd::core::Channel
)";
    EXPECT_EQ(profiler.report_str(), expected);
}

TEST_F(TestTreeProfiler, test_report_links)
{
    Node root("root");
    new Channel("declared", "int", &root);
    LinkingDevice *dev = root.add_child<LinkingDevice>("device");
    dev->add_link("c2", "..declared");

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());

    Channel *declared = root.get_child<Channel>("declared");
    ASSERT_NE(declared, nullptr);

    FakeClock clock;
    TreeMonitor monitor;
    TreeProfiler profiler(monitor);
    monitor.set_clock(&clock);
    ASSERT_TRUE(monitor.observe(&root));

    EXPECT_TRUE(declared->write<int>(0, 1));
    EXPECT_EQ(dev->handled, 1);

    // the link delivers the write to both channels, so both notifications are measured
    const std::string expected = R"(root: sihd::util::Node
  declared: sihd::core::Channel
    writes=1 total=+3ms:0us avg=+3ms:0us min=+3ms:0us max=+3ms:0us
    -> device (test::LinkingDevice): n=1 total=+1ms:0us avg=+1ms:0us min=+1ms:0us max=+1ms:0us
  device: test::LinkingDevice [running]
    c1: sihd::core::Channel
    c2: sihd::core::Channel  => root.declared
      writes=1 total=+3ms:0us avg=+3ms:0us min=+3ms:0us max=+3ms:0us
      -> device (test::LinkingDevice): n=1 total=+1ms:0us avg=+1ms:0us min=+1ms:0us max=+1ms:0us
)";
    EXPECT_EQ(profiler.report_str(), expected);
}

TEST_F(TestTreeProfiler, test_restore_on_reset)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());

    Channel *c1 = dev->get_channel("c1").value_or(nullptr);
    ASSERT_NE(c1, nullptr);

    int waiter_calls = 0;
    Handler<Channel *> waiter([&](Channel *) { ++waiter_calls; });
    c1->add_observer(&waiter);

    TreeMonitor monitor;
    TreeProfiler profiler(monitor);
    ASSERT_TRUE(monitor.observe(&root));
    EXPECT_TRUE(c1->is_observer(&waiter));

    EXPECT_TRUE(c1->write<int>(0, 1));
    EXPECT_EQ(waiter_calls, 1);
    EXPECT_EQ(dev->handled, 1);

    monitor.reset();
    profiler.reset();
    EXPECT_TRUE(c1->is_observer(&waiter));

    EXPECT_TRUE(c1->write<int>(0, 2));
    EXPECT_EQ(waiter_calls, 2);
    EXPECT_EQ(dev->handled, 2);

    EXPECT_EQ(monitor.channels_count(), 0u);
    EXPECT_EQ(monitor.services_count(), 0u);
    EXPECT_EQ(profiler.report_str(), "");
    EXPECT_TRUE(profiler.events().empty());
}

TEST_F(TestTreeProfiler, test_service_op_timing)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");

    FakeClock clock;
    TreeMonitor monitor;
    TreeProfiler profiler(monitor);
    monitor.set_clock(&clock);
    // observed before the lifecycle: operations are timed, channels do not exist yet
    ASSERT_TRUE(monitor.observe(&root));
    EXPECT_EQ(monitor.services_count(), 1u);

    ASSERT_TRUE(dev->setup());
    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());
    ASSERT_TRUE(dev->stop());

    EXPECT_TRUE(monitor.observe(&root));
    EXPECT_EQ(monitor.channels_count(), 2u);

    Channel *c1 = dev->get_channel("c1").value_or(nullptr);
    ASSERT_NE(c1, nullptr);
    EXPECT_TRUE(c1->write<int>(0, 1));

    // operations cost one tick; the stopped device has no observer so the write envelope alone costs one
    const std::string expected = R"(root: sihd::util::Node
  device: test::ProfilingDevice [stopped]
    setup: n=1 total=+1ms:0us avg=+1ms:0us min=+1ms:0us max=+1ms:0us
    init: n=1 total=+1ms:0us avg=+1ms:0us min=+1ms:0us max=+1ms:0us
    start: n=1 total=+1ms:0us avg=+1ms:0us min=+1ms:0us max=+1ms:0us
    stop: n=1 total=+1ms:0us avg=+1ms:0us min=+1ms:0us max=+1ms:0us
    c1: sihd::core::Channel
      writes=1 total=+1ms:0us avg=+1ms:0us min=+1ms:0us max=+1ms:0us
    c2: sihd::core::Channel
)";
    EXPECT_EQ(profiler.report_str(), expected);
}

TEST_F(TestTreeProfiler, test_events)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");

    FakeClock clock;
    TreeMonitor monitor;
    TreeProfiler profiler(monitor);
    monitor.set_clock(&clock);
    profiler.set_max_events(4);
    ASSERT_TRUE(monitor.observe(&root));

    ASSERT_TRUE(dev->setup());
    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());
    ASSERT_TRUE(monitor.observe(&root));

    Channel *c1 = dev->get_channel("c1").value_or(nullptr);
    ASSERT_NE(c1, nullptr);
    EXPECT_TRUE(c1->write<int>(0, 1));
    EXPECT_TRUE(c1->write<int>(0, 2));

    const std::string tid = fmt::format("{}", std::this_thread::get_id());
    const std::string expected = "[" + tid + "] root.device.c1 write ...\n" + "[" + tid
                                 + "] root.device.c1 write = +3ms:0us\n" + "[" + tid + "] root.device.c1 write ...\n"
                                 + "[" + tid + "] root.device.c1 write = +3ms:0us\n";
    EXPECT_EQ(profiler.events_str(), expected);

    const std::vector<TreeProfiler::Event> events = profiler.events();
    ASSERT_EQ(events.size(), 4u);
    EXPECT_EQ(events[0].kind, TreeProfiler::Event::enter);
    EXPECT_EQ(events[0].what, "write");
    EXPECT_EQ(events[0].source, "root.device.c1");
    EXPECT_EQ(events[0].duration, 0);
    EXPECT_EQ(events[1].kind, TreeProfiler::Event::exit);
    EXPECT_EQ(events[1].duration, time::milli(3));
    EXPECT_TRUE(events[1].success);
    EXPECT_EQ(events[3].kind, TreeProfiler::Event::exit);
    EXPECT_EQ(events[3].duration, time::milli(3));

    profiler.clear();
    EXPECT_TRUE(profiler.events().empty());
    EXPECT_EQ(profiler.events_str(), "");
}

TEST_F(TestTreeProfiler, test_event_hook)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");

    FakeClock clock;
    TreeMonitor monitor;
    TreeProfiler profiler(monitor);
    monitor.set_clock(&clock);
    profiler.set_max_events(2);

    EventCollector collector;
    profiler.add_observer(&collector);
    ASSERT_TRUE(monitor.observe(&root));

    ASSERT_TRUE(dev->setup());
    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());
    ASSERT_TRUE(monitor.observe(&root));

    Channel *c1 = dev->get_channel("c1").value_or(nullptr);
    ASSERT_NE(c1, nullptr);
    EXPECT_TRUE(c1->write<int>(0, 1));

    // hooks fire twice per measured action: the enter event then the exit one
    ASSERT_TRUE(monitor.flush());
    ASSERT_EQ(collector.calls, 8);
    const std::vector<std::string> expected_whats =
        {"setup", "setup", "init", "init", "start", "start", "write", "write"};
    EXPECT_EQ(collector.whats, expected_whats);
    const std::vector<TreeProfiler::Event::Kind> expected_kinds = {TreeProfiler::Event::enter,
                                                                   TreeProfiler::Event::exit,
                                                                   TreeProfiler::Event::enter,
                                                                   TreeProfiler::Event::exit,
                                                                   TreeProfiler::Event::enter,
                                                                   TreeProfiler::Event::exit,
                                                                   TreeProfiler::Event::enter,
                                                                   TreeProfiler::Event::exit};
    EXPECT_EQ(collector.kinds, expected_kinds);
    EXPECT_EQ(collector.sources[0], "root.device");
    EXPECT_EQ(collector.sources[7], "root.device.c1");
    EXPECT_EQ(profiler.events().size(), 2u);
    EXPECT_EQ(monitor.dropped_events(), 0u);

    profiler.remove_observer(&collector);
    EXPECT_TRUE(c1->write<int>(0, 2));
    ASSERT_TRUE(monitor.flush());
    EXPECT_EQ(collector.calls, 8);
}

TEST_F(TestTreeProfiler, test_event_hook_reentrancy)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");

    int hook_calls = 0;
    TreeMonitor monitor;
    TreeProfiler profiler(monitor);
    // the hook runs on the dispatch thread after the profiler released its locks, so it may call back
    Handler<TreeProfiler::Event *> hook([&]([[maybe_unused]] TreeProfiler::Event *event) {
        (void)profiler.report_str();
        (void)profiler.events();
        ++hook_calls;
    });
    profiler.add_observer(&hook);
    ASSERT_TRUE(monitor.observe(&root));

    ASSERT_TRUE(dev->setup());
    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());
    ASSERT_TRUE(monitor.observe(&root));

    Channel *c1 = dev->get_channel("c1").value_or(nullptr);
    ASSERT_NE(c1, nullptr);
    EXPECT_TRUE(c1->write<int>(0, 1));

    ASSERT_TRUE(monitor.flush());
    EXPECT_EQ(hook_calls, 8);
}

TEST_F(TestTreeProfiler, test_event_poll)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");

    FakeClock clock;
    TreeMonitor monitor;
    TreeProfiler profiler(monitor);
    monitor.set_clock(&clock);
    profiler.set_max_events(3);
    ASSERT_TRUE(monitor.observe(&root));

    ASSERT_TRUE(dev->setup());
    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());
    ASSERT_TRUE(monitor.observe(&root));

    Channel *c1 = dev->get_channel("c1").value_or(nullptr);
    ASSERT_NE(c1, nullptr);
    EXPECT_TRUE(c1->write<int>(0, 1));

    size_t last_seen = 0;
    const std::vector<TreeProfiler::Event> events = profiler.events();
    ASSERT_EQ(events.size(), 3u);
    for (const TreeProfiler::Event & event : events)
    {
        EXPECT_GT(event.id, last_seen);
        last_seen = event.id;
    }
    EXPECT_EQ(last_seen, 8u);
    EXPECT_EQ(events[2].kind, TreeProfiler::Event::exit);
    EXPECT_EQ(events[2].what, "write");

    EXPECT_TRUE(c1->write<int>(0, 2));
    EXPECT_TRUE(c1->write<int>(0, 3));
    EXPECT_TRUE(c1->write<int>(0, 4));

    const std::vector<TreeProfiler::Event> more = profiler.events();
    ASSERT_EQ(more.size(), 3u);
    EXPECT_EQ(more[0].id, 12u);
    EXPECT_EQ(more[0].kind, TreeProfiler::Event::exit);
    EXPECT_EQ(more[1].kind, TreeProfiler::Event::enter);
    EXPECT_EQ(more[2].id, 14u);
    EXPECT_EQ(more[2].kind, TreeProfiler::Event::exit);
    EXPECT_EQ(more[2].source, "root.device.c1");
    EXPECT_EQ(more[2].duration, time::milli(3));
}

TEST_F(TestTreeProfiler, test_trace)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");

    FakeClock clock;
    TreeMonitor monitor;
    TreeProfiler profiler(monitor);
    monitor.set_clock(&clock);
    ASSERT_TRUE(monitor.observe(&root));
    profiler.set_trace(true);

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());
    ASSERT_TRUE(monitor.observe(&root));

    Channel *c1 = dev->get_channel("c1").value_or(nullptr);
    ASSERT_NE(c1, nullptr);
    EXPECT_TRUE(c1->write<int>(0, 1));
    EXPECT_TRUE(c1->write<int>(0, 2));

    profiler.set_trace(false);
    EXPECT_TRUE(c1->write<int>(0, 3));

    // a notification on c1 costs 3 clock ticks: begin + the device call + end
    const std::string expected = R"(root: sihd::util::Node
  device: test::ProfilingDevice [running]
    init: n=1 total=+1ms:0us avg=+1ms:0us min=+1ms:0us max=+1ms:0us
    start: n=1 total=+1ms:0us avg=+1ms:0us min=+1ms:0us max=+1ms:0us
    c1: sihd::core::Channel
      writes=3 total=+9ms:0us avg=+3ms:0us min=+3ms:0us max=+3ms:0us
      -> device (test::ProfilingDevice): n=3 total=+3ms:0us avg=+1ms:0us min=+1ms:0us max=+1ms:0us
    c2: sihd::core::Channel
)";
    EXPECT_EQ(profiler.report_str(), expected);
}

TEST_F(TestTreeProfiler, test_report_from_service_op)
{
    Node root("root");
    ReportingDevice *dev = root.add_child<ReportingDevice>("device");

    TreeMonitor monitor;
    TreeProfiler profiler(monitor);
    ASSERT_TRUE(monitor.observe(&root));
    dev->set_profiler(&profiler);

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());
    EXPECT_TRUE(dev->is_running());
    EXPECT_TRUE(dev->report().empty());
}

TEST_F(TestTreeProfiler, test_capture_start_on_service)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");

    TreeMonitor monitor;
    TreeProfiler profiler(monitor);
    ASSERT_TRUE(monitor.observe(&root));
    EXPECT_TRUE(profiler.capturing());

    profiler.start_when("device", TreeProfiler::running());
    EXPECT_FALSE(profiler.capturing());

    ASSERT_TRUE(dev->init());
    EXPECT_FALSE(profiler.capturing());

    ASSERT_TRUE(dev->start());
    EXPECT_TRUE(profiler.capturing());

    const std::vector<TreeProfiler::Event> events = profiler.events();
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].what, "start");
    EXPECT_EQ(events[0].kind, TreeProfiler::Event::enter);
    EXPECT_EQ(events[1].what, "start");
    EXPECT_EQ(events[1].kind, TreeProfiler::Event::exit);

    Channel *c1 = dev->get_channel("c1").value_or(nullptr);
    ASSERT_NE(c1, nullptr);
    EXPECT_TRUE(c1->write<int>(0, 1));
    EXPECT_EQ(profiler.events().size(), 4u);
}

TEST_F(TestTreeProfiler, test_capture_start_condition_already_satisfied)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");
    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());

    TreeMonitor monitor;
    TreeProfiler profiler(monitor);
    ASSERT_TRUE(monitor.observe(&root));
    profiler.start_when("device", TreeProfiler::running());
    EXPECT_TRUE(profiler.capturing());
}

TEST_F(TestTreeProfiler, test_capture_multiple_start_conditions)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");
    ProfilingDevice *dev2 = root.add_child<ProfilingDevice>("device2");

    TreeMonitor monitor;
    TreeProfiler profiler(monitor);
    ASSERT_TRUE(monitor.observe(&root));
    profiler.start_when("device", TreeProfiler::running());
    profiler.start_when("device2", TreeProfiler::running());
    EXPECT_FALSE(profiler.capturing());

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());
    EXPECT_FALSE(profiler.capturing());

    ASSERT_TRUE(dev2->init());
    ASSERT_TRUE(dev2->start());
    EXPECT_TRUE(profiler.capturing());
}

TEST_F(TestTreeProfiler, test_capture_stop_on_channel)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");
    new Channel("flag", "bool", &root);
    Channel *flag = root.get_child<Channel>("flag");
    ASSERT_NE(flag, nullptr);

    TreeMonitor monitor;
    TreeProfiler profiler(monitor);
    ASSERT_TRUE(monitor.observe(&root));
    profiler.start_when("device", TreeProfiler::running());
    // the flag is born false: the stop condition is not satisfied at attach
    profiler.stop_when("flag", ChannelMatch::equal(false));
    EXPECT_FALSE(profiler.capturing());

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());
    EXPECT_TRUE(profiler.capturing());

    EXPECT_TRUE(flag->write<bool>(0, true));
    EXPECT_TRUE(profiler.capturing());
    EXPECT_TRUE(flag->write<bool>(0, false));
    EXPECT_FALSE(profiler.capturing());

    EXPECT_TRUE(flag->write<bool>(0, true));
    EXPECT_FALSE(profiler.capturing());

    const std::vector<TreeProfiler::Event> events = profiler.events();
    ASSERT_EQ(events.size(), 6u);
    EXPECT_EQ(events[4].source, "root.flag");
    EXPECT_EQ(events[4].kind, TreeProfiler::Event::enter);
    EXPECT_EQ(events[5].source, "root.flag");
    EXPECT_EQ(events[5].kind, TreeProfiler::Event::exit);
}

TEST_F(TestTreeProfiler, test_capture_stop_on_late_channel)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");

    TreeMonitor monitor;
    TreeProfiler profiler(monitor);
    ASSERT_TRUE(monitor.observe(&root));
    profiler.start_when("device", TreeProfiler::running());
    // c1 is created by init: the path resolves without calling observe again
    profiler.stop_when("device.c1", ChannelMatch::equal(2));
    EXPECT_FALSE(profiler.capturing());

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());
    EXPECT_TRUE(profiler.capturing());

    Channel *c1 = dev->get_channel("c1").value_or(nullptr);
    ASSERT_NE(c1, nullptr);
    EXPECT_TRUE(c1->write<int>(0, 1));
    EXPECT_TRUE(profiler.capturing());
    EXPECT_TRUE(c1->write<int>(0, 2));
    EXPECT_FALSE(profiler.capturing());

    EXPECT_TRUE(c1->write<int>(0, 3));
    EXPECT_FALSE(profiler.capturing());
    EXPECT_EQ(profiler.events().size(), 6u);
}

TEST_F(TestTreeProfiler, test_capture_stop_only)
{
    Node root("root");
    new Channel("flag", "bool", &root);
    Channel *flag = root.get_child<Channel>("flag");
    ASSERT_NE(flag, nullptr);

    TreeMonitor monitor;
    TreeProfiler profiler(monitor);
    ASSERT_TRUE(monitor.observe(&root));
    profiler.stop_when("flag", ChannelMatch::equal(false));
    EXPECT_TRUE(profiler.capturing());

    EXPECT_TRUE(flag->write<bool>(0, true));
    EXPECT_TRUE(profiler.capturing());
    EXPECT_TRUE(flag->write<bool>(0, false));
    EXPECT_FALSE(profiler.capturing());
    EXPECT_EQ(profiler.events().size(), 4u);
}

TEST_F(TestTreeProfiler, test_capture_manual)
{
    Node root("root");
    new Channel("chan", "int", &root);
    Channel *chan = root.get_child<Channel>("chan");
    ASSERT_NE(chan, nullptr);

    TreeMonitor monitor;
    TreeProfiler profiler(monitor);
    ASSERT_TRUE(monitor.observe(&root));
    EXPECT_TRUE(profiler.capturing());

    profiler.stop_capture();
    EXPECT_FALSE(profiler.capturing());
    EXPECT_TRUE(chan->write<int>(0, 1));
    EXPECT_TRUE(profiler.events().empty());

    profiler.start_capture();
    EXPECT_TRUE(profiler.capturing());
    EXPECT_TRUE(chan->write<int>(0, 2));
    EXPECT_EQ(profiler.events().size(), 2u);
}

TEST_F(TestTreeProfiler, test_capture_rearm)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");
    new Channel("flag", "bool", &root);
    Channel *flag = root.get_child<Channel>("flag");
    ASSERT_NE(flag, nullptr);

    TreeMonitor monitor;
    TreeProfiler profiler(monitor);
    profiler.set_rearm(true);
    profiler.start_when("device", TreeProfiler::running());
    profiler.stop_when("flag", ChannelMatch::equal(false));
    ASSERT_TRUE(monitor.observe(&root));
    EXPECT_FALSE(profiler.capturing());

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());
    EXPECT_TRUE(profiler.capturing());

    EXPECT_TRUE(flag->write<bool>(0, true));
    EXPECT_TRUE(flag->write<bool>(0, false));
    EXPECT_FALSE(profiler.capturing());

    ASSERT_TRUE(dev->stop());
    EXPECT_FALSE(profiler.capturing());
    ASSERT_TRUE(dev->start());
    EXPECT_TRUE(profiler.capturing());

    EXPECT_TRUE(flag->write<bool>(0, true));
    EXPECT_TRUE(flag->write<bool>(0, false));
    EXPECT_FALSE(profiler.capturing());
    EXPECT_EQ(profiler.events().size(), 12u);
}

TEST_F(TestTreeProfiler, test_capture_one_shot)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");
    new Channel("flag", "bool", &root);
    Channel *flag = root.get_child<Channel>("flag");
    ASSERT_NE(flag, nullptr);

    TreeMonitor monitor;
    TreeProfiler profiler(monitor);
    profiler.start_when("device", TreeProfiler::running());
    profiler.stop_when("flag", ChannelMatch::equal(false));
    ASSERT_TRUE(monitor.observe(&root));

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());
    // the channel is born false: write_change_only only notifies on a change
    EXPECT_TRUE(flag->write<bool>(0, true));
    EXPECT_TRUE(flag->write<bool>(0, false));
    EXPECT_FALSE(profiler.capturing());

    ASSERT_TRUE(dev->stop());
    ASSERT_TRUE(dev->start());
    EXPECT_FALSE(profiler.capturing());
    EXPECT_TRUE(flag->write<bool>(0, false));
    EXPECT_EQ(profiler.events().size(), 6u);
}

TEST_F(TestTreeProfiler, test_capture_clear_and_reset)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");
    new Channel("flag", "bool", &root);
    Channel *flag = root.get_child<Channel>("flag");
    ASSERT_NE(flag, nullptr);

    TreeMonitor monitor;
    TreeProfiler profiler(monitor);
    profiler.start_when("device", TreeProfiler::running());
    profiler.stop_when("flag", ChannelMatch::equal(false));
    ASSERT_TRUE(monitor.observe(&root));

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());
    // the channel is born false: write_change_only only notifies on a change
    EXPECT_TRUE(flag->write<bool>(0, true));
    EXPECT_TRUE(flag->write<bool>(0, false));
    EXPECT_FALSE(profiler.capturing());

    profiler.clear();
    EXPECT_TRUE(profiler.capturing());
    EXPECT_TRUE(flag->write<bool>(0, true));
    EXPECT_TRUE(flag->write<bool>(0, false));
    EXPECT_FALSE(profiler.capturing());

    // reset drops the conditions: the capture is always on, but nothing is observed anymore
    monitor.reset();
    profiler.reset();
    EXPECT_TRUE(profiler.capturing());
    EXPECT_TRUE(flag->write<bool>(0, true));
    EXPECT_TRUE(profiler.events().empty());
}

TEST_F(TestTreeProfiler, test_session_duration)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");
    new Channel("flag", "bool", &root);
    Channel *flag = root.get_child<Channel>("flag");
    ASSERT_NE(flag, nullptr);

    FakeClock clock;
    TreeMonitor monitor;
    monitor.set_clock(&clock);
    TreeProfiler profiler(monitor);
    profiler.start_when("device", TreeProfiler::running());
    profiler.stop_when("flag", ChannelMatch::equal(false));
    ASSERT_TRUE(monitor.observe(&root));

    EXPECT_EQ(profiler.session_duration().get(), 0);

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());
    const Duration running = profiler.session_duration();
    EXPECT_GT(running.get(), 0);

    flag->write<bool>(0, true);
    flag->write<bool>(0, false);
    EXPECT_FALSE(profiler.capturing());
    const Duration frozen = profiler.session_duration();
    EXPECT_GT(frozen.get(), 0);
    EXPECT_EQ(profiler.session_duration().get(), frozen.get());

    profiler.clear();
    EXPECT_TRUE(profiler.capturing());
    EXPECT_GT(profiler.session_duration().get(), 0);

    profiler.stop_capture();
    const Duration stopped = profiler.session_duration();
    EXPECT_EQ(profiler.session_duration().get(), stopped.get());
    profiler.start_capture();
    EXPECT_GT(profiler.session_duration().get(), 0);
}

TEST_F(TestTreeProfiler, test_reset_from_observer)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());

    Channel *c1 = dev->get_channel("c1").value_or(nullptr);
    ASSERT_NE(c1, nullptr);

    TreeMonitor monitor;
    TreeProfiler profiler(monitor);
    ASSERT_TRUE(monitor.observe(&root));

    // an observer resetting the profiler mid-notification is not called again by the watcher
    Handler<Channel *> resetter([&]([[maybe_unused]] Channel *) {
        monitor.reset();
        profiler.reset();
    });
    c1->add_observer(&resetter);

    EXPECT_TRUE(c1->write<int>(0, 1));
    EXPECT_EQ(dev->handled, 1);
    EXPECT_EQ(monitor.channels_count(), 0u);
    EXPECT_EQ(monitor.services_count(), 0u);
    EXPECT_TRUE(profiler.events().empty());

    EXPECT_TRUE(c1->write<int>(0, 2));
    EXPECT_EQ(dev->handled, 2);
}

TEST_F(TestTreeProfiler, test_event_thread_id)
{
    Node root("root");
    new Channel("c1", "int", &root);
    new Channel("c2", "int", &root);
    Channel *c1 = root.get_child<Channel>("c1");
    Channel *c2 = root.get_child<Channel>("c2");
    ASSERT_NE(c1, nullptr);
    ASSERT_NE(c2, nullptr);

    TreeMonitor monitor;
    TreeProfiler profiler(monitor);
    ASSERT_TRUE(monitor.observe(&root));

    std::thread writer1([&] {
        for (int i = 0; i < 10; ++i)
            EXPECT_TRUE(c1->write<int>(0, i + 1));
    });
    std::thread writer2([&] {
        for (int i = 0; i < 10; ++i)
            EXPECT_TRUE(c2->write<int>(0, i + 1));
    });
    writer1.join();
    writer2.join();

    const std::vector<TreeProfiler::Event> events = profiler.events();
    ASSERT_EQ(events.size(), 40u);
    const TreeProfiler::Event *c1_thread = nullptr;
    const TreeProfiler::Event *c2_thread = nullptr;
    for (const TreeProfiler::Event & event : events)
    {
        if (event.source == "root.c1")
        {
            if (c1_thread == nullptr)
                c1_thread = &event;
            EXPECT_EQ(event.thread_id, c1_thread->thread_id);
        }
        else if (event.source == "root.c2")
        {
            if (c2_thread == nullptr)
                c2_thread = &event;
            EXPECT_EQ(event.thread_id, c2_thread->thread_id);
        }
    }
    ASSERT_NE(c1_thread, nullptr);
    ASSERT_NE(c2_thread, nullptr);
    EXPECT_NE(c1_thread->thread_id, c2_thread->thread_id);

    EXPECT_TRUE(c1->write<int>(0, 100));
    EXPECT_NE(profiler.events().back().thread_id, c1_thread->thread_id);
}

TEST_F(TestTreeProfiler, test_clear_from_observer)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());

    Channel *c1 = dev->get_channel("c1").value_or(nullptr);
    ASSERT_NE(c1, nullptr);

    FakeClock clock;
    TreeMonitor monitor;
    TreeProfiler profiler(monitor);
    monitor.set_clock(&clock);
    ASSERT_TRUE(monitor.observe(&root));

    // clear from an observer leaves the envelope in flight alone; the second clear drops the first sample
    LambdaObserver clearer("clearer", [&](Channel *) { profiler.clear(); });
    c1->add_observer(&clearer);

    EXPECT_TRUE(c1->write<int>(0, 1));
    EXPECT_TRUE(c1->write<int>(0, 2));
    EXPECT_EQ(dev->handled, 2);

    const std::string expected = R"(root: sihd::util::Node
  device: test::ProfilingDevice [running]
    c1: sihd::core::Channel
      writes=1 total=+5ms:0us avg=+5ms:0us min=+5ms:0us max=+5ms:0us
      -> device (test::ProfilingDevice): n=2 total=+2ms:0us avg=+1ms:0us min=+1ms:0us max=+1ms:0us
      -> clearer (test::LambdaObserver): n=2 total=+2ms:0us avg=+1ms:0us min=+1ms:0us max=+1ms:0us
    c2: sihd::core::Channel
)";
    EXPECT_EQ(profiler.report_str(), expected);
}

TEST_F(TestTreeProfiler, test_reset_and_reobserve_from_observer)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());

    Channel *c1 = dev->get_channel("c1").value_or(nullptr);
    ASSERT_NE(c1, nullptr);

    TreeMonitor monitor;
    TreeProfiler profiler(monitor);
    ASSERT_TRUE(monitor.observe(&root));

    // reset then re-observe mid-notification hands the remaining hooks to fresh entries that must ignore them
    bool rebuilt = false;
    LambdaObserver rebuilder("rebuilder", [&](Channel *) {
        if (rebuilt)
            return;
        rebuilt = true;
        monitor.reset();
        profiler.reset();
        monitor.observe(&root);
    });
    c1->add_observer(&rebuilder);

    EXPECT_TRUE(c1->write<int>(0, 1));
    EXPECT_EQ(dev->handled, 1);
    EXPECT_EQ(monitor.channels_count(), 2u);

    EXPECT_TRUE(c1->write<int>(0, 2));
    EXPECT_EQ(dev->handled, 2);

    const std::vector<TreeProfiler::Event> events = profiler.events();
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].what, "write");
    EXPECT_EQ(events[0].kind, TreeProfiler::Event::enter);
    EXPECT_EQ(events[0].source, "root.device.c1");
    EXPECT_EQ(events[1].kind, TreeProfiler::Event::exit);
}

TEST_F(TestTreeProfiler, test_reentrant_write_rejected)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());

    Channel *c1 = dev->get_channel("c1").value_or(nullptr);
    ASSERT_NE(c1, nullptr);

    FakeClock clock;
    TreeMonitor monitor;
    TreeProfiler profiler(monitor);
    monitor.set_clock(&clock);
    ASSERT_TRUE(monitor.observe(&root));

    bool inner_ok = true;
    LambdaObserver selfwriter("selfwriter", [&](Channel *c) { inner_ok = c->write<int>(0, 42); });
    c1->add_observer(&selfwriter);

    EXPECT_TRUE(c1->write<int>(0, 1));
    EXPECT_FALSE(inner_ok);
    EXPECT_EQ(dev->handled, 1);

    const std::string expected = R"(root: sihd::util::Node
  device: test::ProfilingDevice [running]
    c1: sihd::core::Channel
      writes=1 total=+5ms:0us avg=+5ms:0us min=+5ms:0us max=+5ms:0us
      -> device (test::ProfilingDevice): n=1 total=+1ms:0us avg=+1ms:0us min=+1ms:0us max=+1ms:0us
      -> selfwriter (test::LambdaObserver): n=1 total=+1ms:0us avg=+1ms:0us min=+1ms:0us max=+1ms:0us
    c2: sihd::core::Channel
)";
    EXPECT_EQ(profiler.report_str(), expected);
}

TEST_F(TestTreeProfiler, test_cross_channel_write_profiling)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());

    Channel *c1 = dev->get_channel("c1").value_or(nullptr);
    Channel *c2 = dev->get_channel("c2").value_or(nullptr);
    ASSERT_NE(c1, nullptr);
    ASSERT_NE(c2, nullptr);

    FakeClock clock;
    TreeMonitor monitor;
    TreeProfiler profiler(monitor);
    monitor.set_clock(&clock);
    ASSERT_TRUE(monitor.observe(&root));

    LambdaObserver forwarder("forwarder", [&](Channel *) { c2->write<int>(0, 7); });
    c1->add_observer(&forwarder);

    EXPECT_TRUE(c1->write<int>(0, 1));
    EXPECT_EQ(dev->handled, 2);

    // c1's envelope spans the nested c2 notification: begin, device, forwarder (c2 begin + device + end), end
    const std::string expected = R"(root: sihd::util::Node
  device: test::ProfilingDevice [running]
    c1: sihd::core::Channel
      writes=1 total=+9ms:0us avg=+9ms:0us min=+9ms:0us max=+9ms:0us
      -> device (test::ProfilingDevice): n=1 total=+1ms:0us avg=+1ms:0us min=+1ms:0us max=+1ms:0us
      -> forwarder (test::LambdaObserver): n=1 total=+5ms:0us avg=+5ms:0us min=+5ms:0us max=+5ms:0us
    c2: sihd::core::Channel
      writes=1 total=+3ms:0us avg=+3ms:0us min=+3ms:0us max=+3ms:0us
      -> device (test::ProfilingDevice): n=1 total=+1ms:0us avg=+1ms:0us min=+1ms:0us max=+1ms:0us
)";
    EXPECT_EQ(profiler.report_str(), expected);
}

TEST_F(TestTreeProfiler, test_stop_from_observer)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());

    Channel *c1 = dev->get_channel("c1").value_or(nullptr);
    ASSERT_NE(c1, nullptr);

    FakeClock clock;
    TreeMonitor monitor;
    TreeProfiler profiler(monitor);
    monitor.set_clock(&clock);
    ASSERT_TRUE(monitor.observe(&root));

    // an observer stopping the device mid-notification unsubscribes it from the next ones
    bool stopped = false;
    LambdaObserver stopper("stopper", [&](Channel *) {
        if (stopped == false)
        {
            stopped = true;
            dev->stop();
        }
    });
    c1->add_observer(&stopper);

    EXPECT_TRUE(c1->write<int>(0, 1));
    EXPECT_TRUE(c1->write<int>(0, 2));
    EXPECT_EQ(dev->handled, 1);

    const std::string expected = R"(root: sihd::util::Node
  device: test::ProfilingDevice [stopped]
    stop: n=1 total=+1ms:0us avg=+1ms:0us min=+1ms:0us max=+1ms:0us
    c1: sihd::core::Channel
      writes=2 total=+10ms:0us avg=+5ms:0us min=+3ms:0us max=+7ms:0us
      -> stopper (test::LambdaObserver): n=2 total=+4ms:0us avg=+2ms:0us min=+1ms:0us max=+3ms:0us
    c2: sihd::core::Channel
)";
    EXPECT_EQ(profiler.report_str(), expected);
}

TEST_F(TestTreeProfiler, test_two_profilers)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());

    Channel *c1 = dev->get_channel("c1").value_or(nullptr);
    ASSERT_NE(c1, nullptr);

    TreeMonitor monitor;
    TreeProfiler first(monitor);
    TreeProfiler second(monitor);
    second.start_when("device.c1", ChannelMatch::equal(2));
    ASSERT_TRUE(monitor.observe(&root));

    EXPECT_TRUE(c1->write<int>(0, 1));
    EXPECT_TRUE(c1->write<int>(0, 2));

    EXPECT_EQ(first.events().size(), 4u);
    EXPECT_EQ(second.events().size(), 2u);
    EXPECT_TRUE(second.capturing());
}

class ExitSuccessCollector: public IHandler<TreeProfiler::Event *>
{
    public:
        void handle(TreeProfiler::Event *event) override
        {
            if (event->kind == TreeProfiler::Event::exit)
                exit_success[event->what].push_back(event->success);
        }

        std::map<std::string, std::vector<bool>> exit_success;
};

TEST_F(TestTreeProfiler, test_failing_op_exit_is_unsuccessful)
{
    Node root("root");
    FailingStartDevice *dev = root.add_child<FailingStartDevice>("device");

    FakeClock clock;
    TreeMonitor monitor;
    TreeProfiler profiler(monitor);
    monitor.set_clock(&clock);
    ExitSuccessCollector collector;
    profiler.add_observer(&collector);
    ASSERT_TRUE(monitor.observe(&root));

    ASSERT_TRUE(dev->setup());
    ASSERT_TRUE(dev->init());
    EXPECT_FALSE(dev->start());

    Channel *c1 = dev->get_channel("c1").value_or(nullptr);
    ASSERT_NE(c1, nullptr);
    EXPECT_TRUE(c1->write<int>(0, 1));
    ASSERT_TRUE(monitor.flush());

    ASSERT_EQ(collector.exit_success["start"].size(), 1u);
    EXPECT_FALSE(collector.exit_success["start"][0]);
    ASSERT_EQ(collector.exit_success["write"].size(), 1u);
    EXPECT_TRUE(collector.exit_success["write"][0]);
}

TEST_F(TestTreeProfiler, test_report_opts_recursion_and_description)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());

    FakeClock clock;
    TreeMonitor monitor;
    TreeProfiler profiler(monitor);
    monitor.set_clock(&clock);
    ASSERT_TRUE(monitor.observe(&root));

    Channel *c1 = dev->get_channel("c1").value_or(nullptr);
    ASSERT_NE(c1, nullptr);
    EXPECT_TRUE(c1->write<int>(0, 1));

    const std::string full = profiler.report_str();
    EXPECT_NE(full.find("c1"), std::string::npos);

    TreeProfiler::ReportOpts opts;
    opts.max_recursion = 1;
    const std::string truncated = profiler.report_str(opts);
    EXPECT_NE(truncated.find("device"), std::string::npos);
    EXPECT_EQ(truncated.find("c1"), std::string::npos);

    TreeProfiler::ReportOpts desc;
    desc.description = true;
    const std::string with_desc = profiler.report_str(desc);
    EXPECT_NE(with_desc.find("int["), std::string::npos);
    EXPECT_GT(with_desc.size(), full.size());
}

TEST_F(TestTreeProfiler, test_report_during_threaded_writes)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());

    Channel *c1 = dev->get_channel("c1").value_or(nullptr);
    ASSERT_NE(c1, nullptr);

    TreeMonitor monitor;
    TreeProfiler profiler(monitor);
    ASSERT_TRUE(monitor.observe(&root));

    std::thread writer([&] {
        for (int i = 0; i < 100; ++i)
            EXPECT_TRUE(c1->write<int>(0, i + 1));
    });
    for (int i = 0; i < 20; ++i)
        EXPECT_FALSE(profiler.report_str().empty());
    writer.join();

    EXPECT_EQ(dev->handled, 100);
    EXPECT_EQ(profiler.events().size(), 200u);
}

TEST_F(TestTreeProfiler, test_report_after_reclaimed_channel)
{
    Node root("root");
    ReclaimingDevice *dev = root.add_child<ReclaimingDevice>("dev");

    FakeClock clock;
    TreeMonitor monitor;
    monitor.set_clock(&clock);
    monitor.set_queue_max(0);
    EventGate gate;
    Handler<TreeMonitorEvent *> blocker([&](TreeMonitorEvent *) { gate.pass(); });
    monitor.add_observer(&blocker);
    TreeProfiler profiler(monitor);
    ASSERT_TRUE(monitor.observe(&root));

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());
    Channel *c1 = dev->get_child<Channel>("c1");
    ASSERT_NE(c1, nullptr);

    gate.block();
    ASSERT_TRUE(c1->write<int>(0, 1));
    ASSERT_TRUE(gate.wait_held(1));
    for (int i = 0; i < 100; ++i)
        ASSERT_TRUE(c1->write<int>(0, i + 2));
    ASSERT_TRUE(dev->stop());
    gate.release();

    const std::vector<TreeProfiler::Event> events = profiler.events();
    const size_t stale = std::count_if(events.begin(), events.end(), [](const TreeProfiler::Event & event) {
        return event.source == "root.dev.c1";
    });
    // 101 writes of an enter and an exit, then init, start and stop
    EXPECT_EQ(events.size(), 101u * 2 + 6u);
    EXPECT_EQ(stale, 202u);
    const std::string expected = R"(root: sihd::util::Node
  dev: test::ReclaimingDevice [stopped]
    init: n=1 total=+1ms:0us avg=+1ms:0us min=+1ms:0us max=+1ms:0us
    start: n=1 total=+1ms:0us avg=+1ms:0us min=+1ms:0us max=+1ms:0us
    stop: n=1 total=+1ms:0us avg=+1ms:0us min=+1ms:0us max=+1ms:0us
)";
    EXPECT_EQ(profiler.report_str(), expected);
}

TEST_F(TestTreeProfiler, test_destroy_while_writing)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());

    Channel *c1 = dev->get_channel("c1").value_or(nullptr);
    ASSERT_NE(c1, nullptr);

    TreeMonitor monitor;
    ASSERT_TRUE(monitor.observe(&root));

    Synchronizer writing;
    writing.init_sync(2);
    std::atomic<bool> first_event = true;
    Handler<TreeProfiler::Event *> hook([&]([[maybe_unused]] TreeProfiler::Event *event) {
        if (first_event.exchange(false))
            writing.sync();
    });

    std::atomic<bool> stop = false;
    std::atomic<int> writes = 0;
    std::thread writer;
    bool synced = false;
    {
        TreeProfiler profiler(monitor);
        profiler.add_observer(&hook);
        writer = std::thread([&] {
            for (int i = 1; stop == false; ++i)
            {
                EXPECT_TRUE(c1->write<int>(0, i));
                ++writes;
            }
        });
        synced = writing.sync(Duration(time::sec(1)));
    }
    stop = true;
    writer.join();

    EXPECT_TRUE(synced);
    EXPECT_EQ(dev->handled, writes.load());
    EXPECT_TRUE(monitor.flush());
}

TEST_F(TestTreeProfiler, test_pointer_condition_waits_for_observe)
{
    Node root("root");
    new Channel("flag", "bool", &root);
    Channel *flag = root.get_child<Channel>("flag");
    ASSERT_NE(flag, nullptr);

    TreeMonitor monitor;
    TreeProfiler profiler(monitor);
    EXPECT_TRUE(profiler.start_when(flag, ChannelMatch::equal(true)));
    EXPECT_TRUE(flag->write<bool>(0, true));
    EXPECT_FALSE(profiler.capturing());

    ASSERT_TRUE(monitor.observe(&root));
    EXPECT_TRUE(profiler.capturing());
}

} // namespace test

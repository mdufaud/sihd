#include <algorithm>
#include <atomic>
#include <chrono>
#include <clock_helper.hpp>
#include <map>
#include <thread>
#include <utility>

#include <gtest/gtest.h>

#include <sihd/core/Channel.hpp>
#include <sihd/core/Device.hpp>
#include <sihd/core/TreeMonitor.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/LogInfo.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Synchronizer.hpp>
#include <sihd/util/Waitable.hpp>
#include <sihd/util/time.hpp>

#include "tree_helper.hpp"

namespace test
{
using namespace sihd::util;
using namespace sihd::core;

namespace
{

std::string event_str(const TreeMonitor::Event & event)
{
    switch (event.kind)
    {
        case TreeMonitor::Event::channel_attached:
            return "attached " + *event.path;
        case TreeMonitor::Event::service_attached:
            return fmt::format("attached {} [{}]", *event.path, ServiceController::state_str(*event.state));
        case TreeMonitor::Event::channel_detached:
        case TreeMonitor::Event::service_detached:
            return "detached " + *event.path;
        case TreeMonitor::Event::write_begin:
            return fmt::format("write {} = {}", *event.path, event.value != nullptr ? event.value->str() : "?");
        case TreeMonitor::Event::write_end:
        {
            std::string str = fmt::format("written {} in {}", *event.path, Duration(*event.duration).str());
            for (const TreeMonitor::Event::ObserverTime & time : *event.observers)
                str += fmt::format(" | {} {}", *time.observer.label, Duration(time.duration).str());
            return str;
        }
        case TreeMonitor::Event::op_begin:
            return fmt::format("op {} [{}]", *event.path, ServiceController::state_str(*event.state));
        case TreeMonitor::Event::op_end:
            return fmt::format("op {} [{}] in {}{}",
                               *event.path,
                               ServiceController::state_str(*event.state),
                               Duration(*event.duration).str(),
                               *event.success ? "" : " (error)");
    }
    return "";
}

} // namespace

class MonitorCollector: public IHandler<TreeMonitor::Event *>
{
    public:
        void handle(TreeMonitor::Event *event) override
        {
            gate.pass();
            events.push_back(*event);
            ++calls;
        }

        std::vector<std::string> lines() const
        {
            std::vector<std::string> ret;
            for (const TreeMonitor::Event & event : events)
                ret.push_back(event_str(event));
            return ret;
        }

        size_t count(TreeMonitor::Event::Kind kind, const std::string & path) const
        {
            return std::count_if(events.begin(), events.end(), [&](const TreeMonitor::Event & event) {
                return event.kind == kind && *event.path == path;
            });
        }

        EventGate gate;
        int calls = 0;
        std::vector<TreeMonitor::Event> events;
};

class GateObserver: public IHandler<TreeMonitor::Event *>
{
    public:
        GateObserver() { entered.init_sync(2); }

        void handle([[maybe_unused]] TreeMonitor::Event *event) override
        {
            if (_first.exchange(false) == false)
                return;
            entered.sync();
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        Synchronizer entered;

    private:
        std::atomic<bool> _first = true;
};

class LateCallCounter: public IHandler<TreeMonitor::Event *>
{
    public:
        void handle([[maybe_unused]] TreeMonitor::Event *event) override
        {
            if (removed)
                ++late_calls;
        }

        std::atomic<bool> removed = false;
        std::atomic<int> late_calls = 0;
};

class CapturingLogger: public ALogger
{
    public:
        CapturingLogger(): ALogger("capturing") {}

        void log(const LogInfo & info, std::string_view msg) override
        {
            entries.emplace_back(info.level, std::string(msg));
        }

        size_t count(LogLevel level, std::string_view part, size_t from = 0) const
        {
            size_t matches = 0;
            for (size_t i = from; i < entries.size(); ++i)
            {
                if (entries[i].first == level && entries[i].second.find(part) != std::string::npos)
                    ++matches;
            }
            return matches;
        }

        std::vector<std::pair<LogLevel, std::string>> entries;
};

class WalkingDevice: public Device
{
    public:
        WalkingDevice(const std::string & name, Node *parent = nullptr): Device(name, parent) {}

        bool is_running() const override { return _running; }

        void set_monitor(TreeMonitor *monitor) { _monitor = monitor; }

        bool walked = true;

    protected:
        bool on_start() override
        {
            walked = _monitor->walk([](Named *, const Node *, const std::string &, size_t) {});
            _running = true;
            return true;
        }

    private:
        TreeMonitor *_monitor = nullptr;
        bool _running = false;
};

class GatedInitDevice: public Device
{
    public:
        GatedInitDevice(const std::string & name, Node *parent = nullptr): Device(name, parent) {}

        bool is_running() const override { return this->device_state() == ServiceController::Running; }

        bool wait_init_entered()
        {
            return _waitable.wait_for(Duration(time::sec(1)), [this] { return _entered; });
        }

        void release_init()
        {
            {
                auto lock = _waitable.guard();
                _released = true;
            }
            _waitable.notify_all();
        }

    protected:
        bool on_init() override
        {
            {
                auto lock = _waitable.guard();
                _entered = true;
            }
            _waitable.notify_all();
            _waitable.wait([this] { return _released; });
            this->add_channel("c1", "int");
            return true;
        }

    private:
        Waitable _waitable;
        bool _entered = false;
        bool _released = false;
};

void no_visit(Named *, const Node *, const std::string &, size_t) {}

class TestTreeMonitor: public ::testing::Test
{
    protected:
        TestTreeMonitor() { LoggerManager::stream(); }

        virtual ~TestTreeMonitor() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestTreeMonitor, test_observe_attaches)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");
    new Channel("plain", "int", &root);

    FakeClock clock;
    TreeMonitor monitor;
    monitor.set_clock(&clock);
    MonitorCollector collector;
    monitor.add_observer(&collector);
    ASSERT_TRUE(monitor.observe(&root));

    EXPECT_EQ(monitor.channels_count(), 1u);
    EXPECT_EQ(monitor.services_count(), 1u);

    ASSERT_TRUE(dev->init());
    EXPECT_EQ(monitor.channels_count(), 3u);

    ASSERT_TRUE(monitor.flush());
    const std::vector<std::string> expected = {
        "attached root.device [none]",
        "attached root.plain",
        "op root.device [initializing]",
        "op root.device [stopped] in +1ms:0us",
        "attached root.device.c1",
        "attached root.device.c2",
    };
    EXPECT_EQ(collector.lines(), expected);
}

TEST_F(TestTreeMonitor, test_write_events)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());

    Channel *c1 = dev->get_channel("c1").value_or(nullptr);
    ASSERT_NE(c1, nullptr);
    NamedObserver extra("extra");
    c1->add_observer(&extra);

    FakeClock clock;
    TreeMonitor monitor;
    monitor.set_clock(&clock);
    ASSERT_TRUE(monitor.observe(&root));
    MonitorCollector collector;
    monitor.add_observer(&collector);

    EXPECT_TRUE(c1->write<int>(0, 42));

    // each observer costs two clock ticks, the write envelope two more
    ASSERT_TRUE(monitor.flush());
    const std::vector<std::string> expected = {
        "write root.device.c1 = 42",
        "written root.device.c1 in +5ms:0us | device (test::ProfilingDevice) +1ms:0us | extra (test::NamedObserver) "
        "+1ms:0us",
    };
    EXPECT_EQ(collector.lines(), expected);
    EXPECT_EQ(collector.events[0].channel, c1);
    EXPECT_EQ(collector.events[1].observers->at(1).observer.handler, &extra);
    EXPECT_FALSE(collector.events[0].duration.has_value());
    EXPECT_FALSE(collector.events[0].observers.has_value());
    EXPECT_FALSE(collector.events[1].state.has_value());
    EXPECT_FALSE(collector.events[1].value);
}

TEST_F(TestTreeMonitor, test_value_copy_is_exact)
{
    Node root("root");
    new Channel("c1", "int", &root);
    Channel *c1 = root.get_child<Channel>("c1");
    ASSERT_NE(c1, nullptr);

    TreeMonitor monitor;
    ASSERT_TRUE(monitor.observe(&root));
    MonitorCollector collector;
    monitor.add_observer(&collector);

    collector.gate.block();
    EXPECT_TRUE(c1->write<int>(0, 1));
    ASSERT_TRUE(collector.gate.wait_held(1));
    for (int i = 2; i <= 100; ++i)
        EXPECT_TRUE(c1->write<int>(0, i));
    collector.gate.release();
    ASSERT_TRUE(monitor.flush());

    std::vector<std::string> values;
    std::vector<std::string> expected;
    for (const TreeMonitor::Event & event : collector.events)
    {
        if (event.kind == TreeMonitor::Event::write_begin)
            values.push_back(event.value->str());
    }
    for (int i = 1; i <= 100; ++i)
        expected.push_back(std::to_string(i));
    EXPECT_EQ(values, expected);
}

TEST_F(TestTreeMonitor, test_failing_op)
{
    Node root("root");
    FailingStartDevice *dev = root.add_child<FailingStartDevice>("device");

    FakeClock clock;
    TreeMonitor monitor;
    monitor.set_clock(&clock);
    MonitorCollector collector;
    monitor.add_observer(&collector);
    ASSERT_TRUE(monitor.observe(&root));

    ASSERT_TRUE(dev->init());
    EXPECT_FALSE(dev->start());

    ASSERT_TRUE(monitor.flush());
    const std::vector<std::string> expected = {
        "attached root.device [none]",
        "op root.device [initializing]",
        "op root.device [stopped] in +1ms:0us",
        "attached root.device.c1",
        "op root.device [starting]",
        "op root.device [start error] in +1ms:0us (error)",
    };
    EXPECT_EQ(collector.lines(), expected);
}

TEST_F(TestTreeMonitor, test_observer_removal)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());

    Channel *c1 = dev->get_channel("c1").value_or(nullptr);
    ASSERT_NE(c1, nullptr);

    TreeMonitor monitor;
    ASSERT_TRUE(monitor.observe(&root));

    EXPECT_TRUE(dev->stop());
    EXPECT_TRUE(c1->write<int>(0, 1));
    EXPECT_EQ(dev->handled, 0);

    EXPECT_TRUE(dev->start());
    EXPECT_TRUE(c1->write<int>(0, 2));
    EXPECT_EQ(dev->handled, 1);

    monitor.reset();
    EXPECT_TRUE(c1->write<int>(0, 3));
    EXPECT_EQ(dev->handled, 2);
    EXPECT_EQ(monitor.channels_count(), 0u);
    EXPECT_EQ(monitor.services_count(), 0u);
}

TEST_F(TestTreeMonitor, test_event_queue_pressure)
{
    Node root("root");
    new Channel("c1", "int", &root);
    new Channel("c2", "int", &root);
    Channel *c1 = root.get_child<Channel>("c1");
    Channel *c2 = root.get_child<Channel>("c2");
    ASSERT_NE(c1, nullptr);
    ASSERT_NE(c2, nullptr);

    TreeMonitor monitor;
    monitor.set_queue_max(6);
    monitor.set_queue_warning(2);
    ASSERT_TRUE(monitor.observe(&root));

    CapturingLogger logger;
    ASSERT_TRUE(LoggerManager::add(&logger));

    MonitorCollector hook;
    monitor.add_observer(&hook);

    hook.gate.block();
    EXPECT_TRUE(c1->write<int>(0, 1));
    ASSERT_TRUE(hook.gate.wait_held(1));
    EXPECT_TRUE(c2->write<int>(0, 1));
    EXPECT_TRUE(c1->write<int>(0, 2));
    EXPECT_TRUE(c2->write<int>(0, 2));
    EXPECT_TRUE(c1->write<int>(0, 3));
    EXPECT_TRUE(c2->write<int>(0, 3));

    EXPECT_EQ(logger.count(LogLevel::warning, "almost full"), 2u);
    EXPECT_EQ(logger.count(LogLevel::error, "are lost"), 2u);

    hook.gate.release();
    EXPECT_TRUE(monitor.flush());
    // every produced event was either notified or lost: 6 writes = 12 events
    EXPECT_EQ(hook.calls + static_cast<int>(monitor.dropped_events()), 12);

    const size_t entries_before = logger.entries.size();
    monitor.set_queue_max(64);

    hook.gate.block();
    EXPECT_TRUE(c1->write<int>(0, 4));
    ASSERT_TRUE(hook.gate.wait_held(2));
    EXPECT_TRUE(c2->write<int>(0, 4));
    EXPECT_TRUE(c1->write<int>(0, 5));
    EXPECT_TRUE(c2->write<int>(0, 5));

    EXPECT_EQ(logger.count(LogLevel::warning, "almost full", entries_before), 2u);
    EXPECT_EQ(logger.count(LogLevel::error, "are lost", entries_before), 0u);

    hook.gate.release();
    EXPECT_TRUE(monitor.flush());

    LoggerManager::rm(&logger);
}

TEST_F(TestTreeMonitor, test_threaded_writes)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());

    Channel *c1 = dev->get_channel("c1").value_or(nullptr);
    ASSERT_NE(c1, nullptr);

    TreeMonitor monitor;
    ASSERT_TRUE(monitor.observe(&root));
    MonitorCollector collector;
    monitor.add_observer(&collector);

    std::thread writer([&] {
        // the channel starts zeroed: write_on_change would skip a first zero write
        for (int i = 0; i < 100; ++i)
            EXPECT_TRUE(c1->write<int>(0, i + 1));
    });
    for (int i = 0; i < 20; ++i)
        EXPECT_TRUE(monitor.walk(no_visit));
    writer.join();
    ASSERT_TRUE(monitor.flush());

    EXPECT_EQ(dev->handled, 100);
    EXPECT_EQ(collector.count(TreeMonitor::Event::write_end, "root.device.c1"), 100u);
}

TEST_F(TestTreeMonitor, test_concurrent_observe)
{
    Node root("root");
    for (int i = 0; i < 8; ++i)
        new Channel("c" + std::to_string(i), "int", &root);

    TreeMonitor monitor;
    int calls = 0;
    Handler<TreeMonitor::Event *> hook([&]([[maybe_unused]] TreeMonitor::Event *event) { ++calls; });
    monitor.add_observer(&hook);

    // wait for a first observe: CI descheduling can start them past the write loop, making every count vacuous
    Synchronizer observing;
    observing.init_sync(3);
    std::atomic<bool> stop = false;
    auto observe_loop = [&] {
        EXPECT_TRUE(monitor.observe(&root));
        observing.sync();
        while (stop == false)
            EXPECT_TRUE(monitor.observe(&root));
    };
    std::thread observer1(observe_loop);
    std::thread observer2(observe_loop);

    const bool observed = observing.sync(Duration(time::sec(1)));
    for (int i = 0; i < 50; ++i)
    {
        for (const std::string & name : root.children_keys())
        {
            Channel *channel = root.get_child<Channel>(name);
            ASSERT_NE(channel, nullptr);
            ASSERT_TRUE(channel->write<int>(0, i));
        }
    }
    stop = true;
    observer1.join();
    observer2.join();
    ASSERT_TRUE(observed);
    ASSERT_TRUE(monitor.flush());

    // each channel is attached once, then 50 writes of two events: write_on_change skips the zero of the first
    EXPECT_EQ(monitor.channels_count(), 8u);
    EXPECT_EQ(monitor.dropped_events(), 0u);
    EXPECT_EQ(calls, 8 + 8 * 49 * 2);
}

TEST_F(TestTreeMonitor, test_attach_events_never_drop)
{
    Node root("root");
    for (int i = 0; i < 8; ++i)
        new Channel("c" + std::to_string(i), "int", &root);
    Channel *c0 = root.get_child<Channel>("c0");
    ASSERT_NE(c0, nullptr);

    TreeMonitor monitor;
    monitor.set_queue_max(1);
    monitor.set_queue_warning(0);
    MonitorCollector collector;
    monitor.add_observer(&collector);

    collector.gate.block();
    ASSERT_TRUE(monitor.observe(&root));
    ASSERT_TRUE(collector.gate.wait_held(1));
    EXPECT_TRUE(c0->write<int>(0, 1));
    collector.gate.release();
    ASSERT_TRUE(monitor.flush());

    const std::vector<std::string> expected = {
        "attached root.c0",
        "attached root.c1",
        "attached root.c2",
        "attached root.c3",
        "attached root.c4",
        "attached root.c5",
        "attached root.c6",
        "attached root.c7",
    };
    EXPECT_EQ(collector.lines(), expected);
    EXPECT_EQ(monitor.dropped_events(), 2u);
}

TEST_F(TestTreeMonitor, test_detach_events)
{
    Node root("root");
    ReclaimingDevice *dev = root.add_child<ReclaimingDevice>("dev");

    FakeClock clock;
    TreeMonitor monitor;
    monitor.set_clock(&clock);
    MonitorCollector collector;
    monitor.add_observer(&collector);
    ASSERT_TRUE(monitor.observe(&root));

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());
    ASSERT_TRUE(dev->stop());
    EXPECT_EQ(monitor.channels_count(), 0u);

    ASSERT_TRUE(monitor.flush());
    const std::vector<std::string> expected = {
        "attached root.dev [none]",
        "op root.dev [initializing]",
        "op root.dev [stopped] in +1ms:0us",
        "attached root.dev.c1",
        "op root.dev [starting]",
        "op root.dev [running] in +1ms:0us",
        "op root.dev [stopping]",
        "op root.dev [stopped] in +1ms:0us",
        "detached root.dev.c1",
    };
    EXPECT_EQ(collector.lines(), expected);
}

TEST_F(TestTreeMonitor, test_rescan_keeps_max_recursion)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");

    TreeMonitor monitor;
    ASSERT_TRUE(monitor.observe(&root, 1));
    ASSERT_TRUE(dev->init());

    EXPECT_EQ(monitor.services_count(), 1u);
    EXPECT_EQ(monitor.channels_count(), 0u);
}

TEST_F(TestTreeMonitor, test_stale_events_after_children_destruction)
{
    Node root("root");
    ReclaimingDevice *dev = root.add_child<ReclaimingDevice>("dev");

    TreeMonitor monitor;
    monitor.set_queue_max(0);
    MonitorCollector hook;
    monitor.add_observer(&hook);
    ASSERT_TRUE(monitor.observe(&root));

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());
    ASSERT_EQ(monitor.channels_count(), 1u);
    Channel *c1 = dev->get_child<Channel>("c1");
    ASSERT_NE(c1, nullptr);

    hook.gate.block();
    ASSERT_TRUE(c1->write<int>(0, 1));
    ASSERT_TRUE(hook.gate.wait_held(1));
    for (int i = 0; i < 100; ++i)
        ASSERT_TRUE(c1->write<int>(0, i + 2));

    // the service stops and reclaims its channel while events are stacked: queued events turn stale
    ASSERT_TRUE(dev->stop());
    EXPECT_EQ(dev->get_child<Channel>("c1"), nullptr);
    EXPECT_EQ(monitor.channels_count(), 0u);

    hook.gate.release();
    ASSERT_TRUE(monitor.flush());

    // 101 writes, init/start/stop ops, the device and its channel attached, the channel detached
    EXPECT_EQ(hook.calls, 101 * 2 + 6 + 2 + 1);
    EXPECT_EQ(hook.count(TreeMonitor::Event::write_end, "root.dev.c1"), 101u);
    EXPECT_EQ(monitor.dropped_events(), 0u);
    EXPECT_EQ(monitor.find("dev.c1"), nullptr);
}

TEST_F(TestTreeMonitor, test_stale_events_interleave_with_live_ones)
{
    Node root("root");
    ReclaimingDevice *dev_a = root.add_child<ReclaimingDevice>("a");
    ReclaimingDevice *dev_b = root.add_child<ReclaimingDevice>("b");

    TreeMonitor monitor;
    monitor.set_queue_max(0);
    MonitorCollector hook;
    monitor.add_observer(&hook);
    ASSERT_TRUE(monitor.observe(&root));
    ASSERT_TRUE(dev_a->init());
    ASSERT_TRUE(dev_b->init());
    ASSERT_TRUE(dev_a->start());
    ASSERT_TRUE(dev_b->start());
    Channel *ac1 = dev_a->get_child<Channel>("c1");
    Channel *bc1 = dev_b->get_child<Channel>("c1");
    ASSERT_NE(ac1, nullptr);
    ASSERT_NE(bc1, nullptr);

    hook.gate.block();
    ASSERT_TRUE(ac1->write<int>(0, 1));
    ASSERT_TRUE(hook.gate.wait_held(1));
    for (int i = 0; i < 50; ++i)
    {
        ASSERT_TRUE(ac1->write<int>(0, i + 2));
        ASSERT_TRUE(bc1->write<int>(0, i + 2));
    }
    ASSERT_TRUE(dev_a->stop());
    for (int i = 0; i < 10; ++i)
        ASSERT_TRUE(bc1->write<int>(0, i + 52));

    hook.gate.release();
    ASSERT_TRUE(monitor.flush());

    // writes: 51 on a.c1, 60 on b.c1; ops: 3 on a, 2 on b; 2 devices and 2 channels attached; a.c1 detached
    EXPECT_EQ(hook.calls, 51 * 2 + 60 * 2 + 6 + 4 + 4 + 1);

    size_t first_fresh = hook.events.size();
    size_t last_stale = 0;
    for (size_t i = 0; i < hook.events.size(); ++i)
    {
        if (hook.events[i].kind != TreeMonitor::Event::write_end)
            continue;
        if (*hook.events[i].path == "root.a.c1")
            last_stale = i;
        else if (*hook.events[i].path == "root.b.c1" && first_fresh == hook.events.size())
            first_fresh = i;
    }
    EXPECT_EQ(hook.count(TreeMonitor::Event::write_end, "root.a.c1"), 51u);
    EXPECT_EQ(hook.count(TreeMonitor::Event::write_end, "root.b.c1"), 60u);
    EXPECT_LT(first_fresh, last_stale);
    EXPECT_EQ(monitor.dropped_events(), 0u);
    EXPECT_EQ(monitor.channels_count(), 1u);
    EXPECT_EQ(monitor.services_count(), 2u);
}

TEST_F(TestTreeMonitor, test_walk_stops_chain_in_op_start)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");
    AService *service = dev;
    auto *ctrl = dynamic_cast<ServiceController *>(service->service_ctrl());
    ASSERT_NE(ctrl, nullptr);

    Synchronizer starting;
    starting.init_sync(2);
    Handler<ServiceController *> start_spy([&](ServiceController *controller) {
        if (controller->state() == ServiceController::Starting)
            starting.sync();
    });
    ctrl->add_observer(&start_spy);

    TreeMonitor monitor;
    MonitorCollector hook;
    monitor.add_observer(&hook);
    ASSERT_TRUE(monitor.observe(&root));
    ASSERT_TRUE(dev->init());

    Synchronizer walking;
    walking.init_sync(2);
    EventGate gate;
    gate.block();
    std::thread walker([&] {
        bool first = true;
        EXPECT_TRUE(monitor.walk([&](Named *, const Node *, const std::string &, size_t) {
            if (std::exchange(first, false))
            {
                walking.sync();
                gate.pass();
            }
        }));
    });
    const bool walk_held = walking.sync(Duration(time::sec(1)));
    std::thread starter([&] { EXPECT_TRUE(dev->start()); });
    const bool start_held = starting.sync(Duration(time::sec(1)));
    EXPECT_FALSE(dev->is_running());

    gate.release();
    starter.join();
    walker.join();
    ctrl->remove_observer(&start_spy);

    EXPECT_TRUE(walk_held);
    EXPECT_TRUE(start_held);
    EXPECT_TRUE(dev->is_running());
    ASSERT_TRUE(monitor.flush());
    EXPECT_EQ(hook.count(TreeMonitor::Event::op_begin, "root.device"), 2u);
    EXPECT_EQ(hook.count(TreeMonitor::Event::op_end, "root.device"), 2u);
}

TEST_F(TestTreeMonitor, test_walk_waits_for_in_flight_op)
{
    Node root("root");
    GatedInitDevice *dev = root.add_child<GatedInitDevice>("device");

    TreeMonitor monitor;
    ASSERT_TRUE(monitor.observe(&root));

    std::vector<std::string> visited;
    const TreeMonitor::Visitor visit = [&](Named *, const Node *, const std::string & key, size_t depth) {
        visited.push_back(std::string(depth * 2, ' ') + key);
    };

    std::thread init([&] { EXPECT_TRUE(dev->init()); });
    const bool entered = dev->wait_init_entered();
    EXPECT_FALSE(monitor.walk(visit, 0, Duration(time::milli(50))));
    EXPECT_TRUE(visited.empty());

    bool walked = false;
    std::thread walker([&] { walked = monitor.walk(visit); });
    dev->release_init();
    init.join();
    walker.join();

    ASSERT_TRUE(entered);
    EXPECT_TRUE(walked);
    const std::vector<std::string> expected = {"root", "  device", "    c1"};
    EXPECT_EQ(visited, expected);
    EXPECT_EQ(monitor.channels_count(), 1u);
}

TEST_F(TestTreeMonitor, test_reset_during_op)
{
    Node root("root");
    GatedInitDevice *dev = root.add_child<GatedInitDevice>("device");

    TreeMonitor monitor;
    ASSERT_TRUE(monitor.observe(&root));

    std::thread init([&] { EXPECT_TRUE(dev->init()); });
    const bool entered = dev->wait_init_entered();
    monitor.reset();
    dev->release_init();
    init.join();

    ASSERT_TRUE(entered);
    ASSERT_TRUE(monitor.observe(&root));
    EXPECT_TRUE(monitor.walk(no_visit));
    EXPECT_EQ(monitor.channels_count(), 1u);
}

TEST_F(TestTreeMonitor, test_walk_from_service_op)
{
    Node root("root");
    WalkingDevice *dev = root.add_child<WalkingDevice>("device");

    TreeMonitor monitor;
    ASSERT_TRUE(monitor.observe(&root));
    dev->set_monitor(&monitor);

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());
    EXPECT_TRUE(dev->is_running());
    EXPECT_FALSE(dev->walked);
}

TEST_F(TestTreeMonitor, test_find)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");
    ASSERT_TRUE(dev->init());
    Channel *c1 = dev->get_channel("c1").value_or(nullptr);
    ASSERT_NE(c1, nullptr);

    TreeMonitor monitor;
    EXPECT_EQ(monitor.find("device"), nullptr);
    EXPECT_FALSE(monitor.observes(dev));
    EXPECT_FALSE(monitor.observes(c1));
    ASSERT_TRUE(monitor.observe(&root));

    EXPECT_EQ(monitor.find("device"), dev);
    EXPECT_EQ(monitor.find("root.device"), dev);
    EXPECT_EQ(monitor.find("device.c1"), c1);
    EXPECT_EQ(monitor.find("device.nope"), nullptr);
    EXPECT_TRUE(monitor.observes(dev));
    EXPECT_TRUE(monitor.observes(c1));
}

TEST_F(TestTreeMonitor, test_threaded_event_order)
{
    Node root("root");
    new Channel("c1", "int", &root);
    new Channel("c2", "int", &root);
    Channel *c1 = root.get_child<Channel>("c1");
    Channel *c2 = root.get_child<Channel>("c2");
    ASSERT_NE(c1, nullptr);
    ASSERT_NE(c2, nullptr);

    TreeMonitor monitor;
    ASSERT_TRUE(monitor.observe(&root));
    MonitorCollector collector;
    monitor.add_observer(&collector);

    std::thread writer1([&] {
        for (int i = 0; i < 50; ++i)
            EXPECT_TRUE(c1->write<int>(0, i + 1));
    });
    std::thread writer2([&] {
        for (int i = 0; i < 50; ++i)
            EXPECT_TRUE(c2->write<int>(0, i + 1));
    });
    writer1.join();
    writer2.join();

    ASSERT_TRUE(monitor.flush());
    EXPECT_EQ(collector.calls, 200);
    std::map<std::string, TreeMonitor::Event::Kind> last;
    for (const TreeMonitor::Event & event : collector.events)
    {
        auto it = last.find(*event.path);
        if (it != last.end())
        {
            EXPECT_NE(it->second, event.kind);
        }
        last[*event.path] = event.kind;
    }
}

TEST_F(TestTreeMonitor, test_two_monitors)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());

    Channel *c1 = dev->get_channel("c1").value_or(nullptr);
    ASSERT_NE(c1, nullptr);

    CapturingLogger logger;
    ASSERT_TRUE(LoggerManager::add(&logger));

    FakeClock first_clock;
    TreeMonitor first;
    first.set_clock(&first_clock);
    ASSERT_TRUE(first.observe(&root));
    MonitorCollector first_events;
    first.add_observer(&first_events);

    FakeClock second_clock;
    TreeMonitor second;
    second.set_clock(&second_clock);
    ASSERT_TRUE(second.observe(&root));
    MonitorCollector second_events;
    second.add_observer(&second_events);
    EXPECT_EQ(first.channels_count(), 2u);
    EXPECT_EQ(second.channels_count(), 0u);
    EXPECT_EQ(first.services_count(), 1u);
    EXPECT_EQ(second.services_count(), 1u);

    EXPECT_TRUE(c1->write<int>(0, 1));
    EXPECT_EQ(dev->handled, 1);

    ASSERT_TRUE(dev->stop());
    ASSERT_TRUE(dev->start());
    ASSERT_TRUE(first.flush());
    ASSERT_TRUE(second.flush());
    LoggerManager::rm(&logger);

    const std::vector<std::string> first_expected = {
        "write root.device.c1 = 1",
        "written root.device.c1 in +3ms:0us | device (test::ProfilingDevice) +1ms:0us",
        "op root.device [stopping]",
        "op root.device [stopped] in +1ms:0us",
        "op root.device [starting]",
        "op root.device [running] in +1ms:0us",
    };
    EXPECT_EQ(first_events.lines(), first_expected);
    const std::vector<std::string> second_expected = {
        "op root.device [stopping]",
        "op root.device [stopped] in +1ms:0us",
        "op root.device [starting]",
        "op root.device [running] in +1ms:0us",
    };
    EXPECT_EQ(second_events.lines(), second_expected);
    EXPECT_EQ(logger.count(LogLevel::warning, "is already watched"), 2u);
}

TEST_F(TestTreeMonitor, test_reset_from_observer)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());

    Channel *c1 = dev->get_channel("c1").value_or(nullptr);
    ASSERT_NE(c1, nullptr);

    TreeMonitor monitor;
    ASSERT_TRUE(monitor.observe(&root));
    MonitorCollector collector;
    monitor.add_observer(&collector);

    Handler<Channel *> resetter([&]([[maybe_unused]] Channel *) { monitor.reset(); });
    c1->add_observer(&resetter);

    EXPECT_TRUE(c1->write<int>(0, 1));
    EXPECT_EQ(dev->handled, 1);
    EXPECT_EQ(monitor.channels_count(), 0u);

    EXPECT_TRUE(c1->write<int>(0, 2));
    EXPECT_EQ(dev->handled, 2);
    ASSERT_TRUE(monitor.flush());
    const std::vector<std::string> expected = {
        "write root.device.c1 = 1",
        "detached root.device",
        "detached root.device.c1",
        "detached root.device.c2",
    };
    EXPECT_EQ(collector.lines(), expected);
}

TEST_F(TestTreeMonitor, test_destroy_while_writing)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());

    Channel *c1 = dev->get_channel("c1").value_or(nullptr);
    ASSERT_NE(c1, nullptr);

    Synchronizer writing;
    writing.init_sync(2);
    std::atomic<bool> first_write = true;
    Handler<TreeMonitor::Event *> hook([&](TreeMonitor::Event *event) {
        if (event->kind == TreeMonitor::Event::write_begin && first_write.exchange(false))
            writing.sync();
    });

    std::atomic<bool> stop = false;
    std::atomic<int> writes = 0;
    std::thread writer;
    bool synced = false;
    {
        TreeMonitor monitor;
        monitor.add_observer(&hook);
        ASSERT_TRUE(monitor.observe(&root));
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
}

TEST_F(TestTreeMonitor, test_reset_from_dispatch_hook)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());

    Channel *c1 = dev->get_channel("c1").value_or(nullptr);
    ASSERT_NE(c1, nullptr);

    TreeMonitor monitor;
    ASSERT_TRUE(monitor.observe(&root));
    int hook_calls = 0;
    Handler<TreeMonitor::Event *> hook([&]([[maybe_unused]] TreeMonitor::Event *) {
        ++hook_calls;
        monitor.reset();
    });
    monitor.add_observer(&hook);

    EXPECT_TRUE(c1->write<int>(0, 1));
    ASSERT_TRUE(monitor.flush());
    EXPECT_EQ(monitor.channels_count(), 0u);
    EXPECT_EQ(monitor.services_count(), 0u);

    EXPECT_TRUE(c1->write<int>(0, 2));
    EXPECT_EQ(dev->handled, 2);
    ASSERT_TRUE(monitor.flush());
    // the first write begin and end, then the device and its two channels detached
    EXPECT_EQ(hook_calls, 5);
}

TEST_F(TestTreeMonitor, test_removed_observer_not_called)
{
    Node root("root");
    new Channel("c1", "int", &root);
    Channel *c1 = root.get_child<Channel>("c1");
    ASSERT_NE(c1, nullptr);

    TreeMonitor monitor;
    ASSERT_TRUE(monitor.observe(&root));
    GateObserver gate;
    LateCallCounter counter;
    monitor.add_observer(&gate);
    monitor.add_observer(&counter);

    EXPECT_TRUE(c1->write<int>(0, 1));
    ASSERT_TRUE(gate.entered.sync(Duration(time::sec(1))));
    monitor.remove_observer(&counter);
    counter.removed = true;
    ASSERT_TRUE(monitor.flush());
    EXPECT_EQ(counter.late_calls, 0);
}

} // namespace test

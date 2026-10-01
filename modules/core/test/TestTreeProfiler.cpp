#include <atomic>
#include <chrono>
#include <clock_helper.hpp>
#include <functional>
#include <future>
#include <thread>
#include <wait_for.hpp>

#include <gtest/gtest.h>

#include <sihd/core/Channel.hpp>
#include <sihd/core/Device.hpp>
#include <sihd/core/TreeProfiler.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/LogInfo.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/thread.hpp>
#include <sihd/util/time.hpp>

namespace test
{
using namespace sihd::util;
using namespace sihd::core;

class NamedObserver: public sihd::util::Named,
                     public sihd::util::IHandler<Channel *>
{
    public:
        NamedObserver(const std::string & name): Named(name) {}

        void handle(Channel *c) override
        {
            ++calls;
            (void)c;
        }

        int calls = 0;
};

class LambdaObserver: public sihd::util::Named,
                      public sihd::util::IHandler<Channel *>
{
    public:
        LambdaObserver(const std::string & name, std::function<void(Channel *)> fun): Named(name), _fun(std::move(fun))
        {
        }

        void handle(Channel *c) override { _fun(c); }

    private:
        std::function<void(Channel *)> _fun;
};

class ProfilingDevice: public Device
{
    public:
        ProfilingDevice(const std::string & name, Node *parent = nullptr): Device(name, parent) {}

        bool is_running() const override { return _running; }

        void handle(Channel *c) override
        {
            ++handled;
            Device::handle(c);
        }

        int handled = 0;

    protected:
        bool on_init() override
        {
            this->add_channel("c1", "int");
            this->add_channel("c2", "int");
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

// reclaims its channels when the service stops: their pointers die on the state change
class ReclaimingDevice: public Device
{
    public:
        ReclaimingDevice(const std::string & name, Node *parent = nullptr): Device(name, parent) {}

        bool is_running() const override { return this->device_state() == ServiceController::Running; }

    protected:
        bool on_init() override
        {
            this->add_channel("c1", "int");
            return true;
        }

        bool on_stop() override
        {
            this->remove_child("c1");
            return true;
        }
};

// holds its init body until released: simulates an op caught in flight
class GatedInitDevice: public Device
{
    public:
        GatedInitDevice(const std::string & name, Node *parent = nullptr): Device(name, parent) {}

        bool is_running() const override { return this->device_state() == ServiceController::Running; }

        void release_init() { _init_released = true; }

    protected:
        bool on_init() override
        {
            while (_init_released == false)
                time::msleep(1);
            this->add_channel("c1", "int");
            return true;
        }

    private:
        std::atomic<bool> _init_released = false;
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

// holds the dispatch thread while block is set: simulates an observer too slow for the producers
class BlockingObserver: public sihd::util::IHandler<TreeProfiler::Event *>
{
    public:
        void handle([[maybe_unused]] TreeProfiler::Event *event) override
        {
            if (block)
                ++held;
            while (block)
                time::msleep(1);
            ++calls;
        }

        std::atomic<bool> block = false;
        std::atomic<int> held = 0;
        int calls = 0;
};

class EventCollector: public sihd::util::IHandler<TreeProfiler::Event *>
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
        TestTreeProfiler() { sihd::util::LoggerManager::stream(); }

        virtual ~TestTreeProfiler() { sihd::util::LoggerManager::clear_loggers(); }

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
    TreeProfiler profiler;
    profiler.set_clock(&clock);
    ASSERT_TRUE(profiler.observe(&root));

    // the two device channels plus the standalone one
    EXPECT_EQ(profiler.channels_count(), 3u);
    EXPECT_EQ(profiler.services_count(), 1u);

    EXPECT_TRUE(c1->write<int>(0, 1));
    EXPECT_TRUE(c1->write<int>(0, 2));
    EXPECT_TRUE(c1->write<int>(0, 3));
    EXPECT_TRUE(c2->write<int>(0, 1));

    EXPECT_EQ(dev->handled, 4);
    EXPECT_EQ(extra.calls, 3);

    // a notification on c1 costs 5 clock ticks: begin + two observer calls + end;
    // on c2: begin + one observer call + end
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
    TreeProfiler profiler;
    profiler.set_clock(&clock);
    ASSERT_TRUE(profiler.observe(&root));

    NamedObserver late("late");
    Channel *c1 = dev->get_channel("c1").value_or(nullptr);
    ASSERT_NE(c1, nullptr);
    c1->add_observer(&late);

    EXPECT_TRUE(c1->write<int>(0, 1));
    EXPECT_EQ(late.calls, 1);

    // two observer calls inside the envelope measure 5ms; c2 sees no write so it
    // stays bare
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
    TreeProfiler profiler;
    profiler.set_clock(&clock);
    ASSERT_TRUE(profiler.observe(&root));

    EXPECT_TRUE(declared->write<int>(0, 1));
    EXPECT_EQ(dev->handled, 1);

    // the link delivers the write to both channels' observer lists, so both
    // notifications are measured; the linked channel shows the link annotation
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

    TreeProfiler profiler;
    ASSERT_TRUE(profiler.observe(&root));
    // the profiler never touches the observers list
    EXPECT_TRUE(c1->is_observer(&waiter));

    EXPECT_TRUE(c1->write<int>(0, 1));
    EXPECT_EQ(waiter_calls, 1);
    EXPECT_EQ(dev->handled, 1);

    profiler.reset();
    EXPECT_TRUE(c1->is_observer(&waiter));

    EXPECT_TRUE(c1->write<int>(0, 2));
    EXPECT_EQ(waiter_calls, 2);
    EXPECT_EQ(dev->handled, 2);

    EXPECT_EQ(profiler.channels_count(), 0u);
    EXPECT_EQ(profiler.services_count(), 0u);
    EXPECT_EQ(profiler.report_str(), "");
    EXPECT_TRUE(profiler.events().empty());
}

TEST_F(TestTreeProfiler, test_observer_removal)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());

    Channel *c1 = dev->get_channel("c1").value_or(nullptr);
    ASSERT_NE(c1, nullptr);

    TreeProfiler profiler;
    ASSERT_TRUE(profiler.observe(&root));

    EXPECT_TRUE(dev->stop());
    EXPECT_TRUE(c1->write<int>(0, 1));
    EXPECT_EQ(dev->handled, 0);

    EXPECT_TRUE(dev->start());
    EXPECT_TRUE(c1->write<int>(0, 2));
    EXPECT_EQ(dev->handled, 1);

    profiler.reset();
    EXPECT_TRUE(c1->write<int>(0, 3));
    EXPECT_EQ(dev->handled, 2);
}

TEST_F(TestTreeProfiler, test_service_op_timing)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");

    FakeClock clock;
    TreeProfiler profiler;
    profiler.set_clock(&clock);
    // observed before the lifecycle: operations are timed, channels do not exist yet
    ASSERT_TRUE(profiler.observe(&root));
    EXPECT_EQ(profiler.services_count(), 1u);

    ASSERT_TRUE(dev->setup());
    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());
    ASSERT_TRUE(dev->stop());

    EXPECT_TRUE(profiler.observe(&root));
    EXPECT_EQ(profiler.channels_count(), 2u);

    Channel *c1 = dev->get_channel("c1").value_or(nullptr);
    ASSERT_NE(c1, nullptr);
    EXPECT_TRUE(c1->write<int>(0, 1));

    // operations cost one clock tick; the write reaches no observer (the device
    // is stopped) so the envelope alone measures it: one tick as well
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
    TreeProfiler profiler;
    profiler.set_clock(&clock);
    profiler.set_max_events(4);
    ASSERT_TRUE(profiler.observe(&root));

    ASSERT_TRUE(dev->setup());
    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());
    ASSERT_TRUE(profiler.observe(&root));

    Channel *c1 = dev->get_channel("c1").value_or(nullptr);
    ASSERT_NE(c1, nullptr);
    EXPECT_TRUE(c1->write<int>(0, 1));
    EXPECT_TRUE(c1->write<int>(0, 2));

    // strace-like trace: an enter event per action then its exit carrying the
    // duration, prefixed with the producer thread id; the ring kept the four
    // events of the two writes
    const std::string tid = thread::id_str();
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
    TreeProfiler profiler;
    profiler.set_clock(&clock);
    profiler.set_max_events(2);

    EventCollector collector;
    profiler.add_observer(&collector);
    ASSERT_TRUE(profiler.observe(&root));

    ASSERT_TRUE(dev->setup());
    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());
    ASSERT_TRUE(profiler.observe(&root));

    Channel *c1 = dev->get_channel("c1").value_or(nullptr);
    ASSERT_NE(c1, nullptr);
    EXPECT_TRUE(c1->write<int>(0, 1));

    // hooks fire twice per measured action: the enter event then the exit one
    ASSERT_TRUE(profiler.flush());
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
    EXPECT_EQ(profiler.dropped_events(), 0u);

    profiler.remove_observer(&collector);
    EXPECT_TRUE(c1->write<int>(0, 2));
    ASSERT_TRUE(profiler.flush());
    EXPECT_EQ(collector.calls, 8);
}

TEST_F(TestTreeProfiler, test_event_hook_reentrancy)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");

    int hook_calls = 0;
    TreeProfiler profiler;
    // the hook runs on the dispatch thread, after the profiler released its locks,
    // so it may call back into it
    Handler<TreeProfiler::Event *> hook([&]([[maybe_unused]] TreeProfiler::Event *event) {
        (void)profiler.report_str();
        (void)profiler.events();
        ++hook_calls;
    });
    profiler.add_observer(&hook);
    ASSERT_TRUE(profiler.observe(&root));

    ASSERT_TRUE(dev->setup());
    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());
    ASSERT_TRUE(profiler.observe(&root));

    Channel *c1 = dev->get_channel("c1").value_or(nullptr);
    ASSERT_NE(c1, nullptr);
    EXPECT_TRUE(c1->write<int>(0, 1));

    ASSERT_TRUE(profiler.flush());
    EXPECT_EQ(hook_calls, 8);
}

TEST_F(TestTreeProfiler, test_event_poll)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");

    FakeClock clock;
    TreeProfiler profiler;
    profiler.set_clock(&clock);
    profiler.set_max_events(3);
    ASSERT_TRUE(profiler.observe(&root));

    ASSERT_TRUE(dev->setup());
    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());
    ASSERT_TRUE(profiler.observe(&root));

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
    // the ring ends on the write enter/exit pair
    EXPECT_EQ(last_seen, 8u);
    EXPECT_EQ(events[2].kind, TreeProfiler::Event::exit);
    EXPECT_EQ(events[2].what, "write");

    EXPECT_TRUE(c1->write<int>(0, 2));
    EXPECT_TRUE(c1->write<int>(0, 3));
    EXPECT_TRUE(c1->write<int>(0, 4));

    // the ring dropped the old entries but ids stay monotonic
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

class CapturingLogger: public sihd::util::ALogger
{
    public:
        void log(const sihd::util::LogInfo & info, std::string_view msg) override
        {
            entries.emplace_back(info.level, std::string(msg));
        }

        std::vector<std::pair<sihd::util::LogLevel, std::string>> entries;
};

TEST_F(TestTreeProfiler, test_event_queue_pressure)
{
    Node root("root");
    new Channel("c1", "int", &root);
    new Channel("c2", "int", &root);
    Channel *c1 = root.get_child<Channel>("c1");
    Channel *c2 = root.get_child<Channel>("c2");
    ASSERT_NE(c1, nullptr);
    ASSERT_NE(c2, nullptr);

    TreeProfiler profiler;
    profiler.set_queue_max(6);
    profiler.set_queue_warning(2);

    CapturingLogger logger;
    ASSERT_TRUE(sihd::util::LoggerManager::add(&logger));

    BlockingObserver hook;
    profiler.add_observer(&hook);
    ASSERT_TRUE(profiler.observe(&root));

    // the dispatch thread is held by the first event: the queue fills up
    hook.block = true;
    EXPECT_TRUE(c1->write<int>(0, 1));
    ASSERT_TRUE(wait_for([&] { return hook.held >= 1; }));
    EXPECT_TRUE(c2->write<int>(0, 1));
    EXPECT_TRUE(c1->write<int>(0, 2));
    EXPECT_TRUE(c2->write<int>(0, 2));
    EXPECT_TRUE(c1->write<int>(0, 3));
    EXPECT_TRUE(c2->write<int>(0, 3));

    // one warning and one error per source while the pressure lasts
    size_t warned = 0;
    size_t lost = 0;
    for (const auto & [level, msg] : logger.entries)
    {
        if (level == sihd::util::LogLevel::warning && msg.find("almost full") != std::string::npos)
            ++warned;
        if (level == sihd::util::LogLevel::error && msg.find("are lost") != std::string::npos)
            ++lost;
    }
    EXPECT_EQ(warned, 2u);
    EXPECT_EQ(lost, 2u);

    hook.block = false;
    EXPECT_TRUE(profiler.flush());
    // every produced event was either notified or lost: 6 writes = 12 events
    EXPECT_EQ(hook.calls + static_cast<int>(profiler.dropped_events()), 12);

    // the queue drained: sources are logged once again at the next pressure
    const size_t entries_before = logger.entries.size();
    profiler.set_queue_max(64);
    hook.block = true;
    EXPECT_TRUE(c1->write<int>(0, 4));
    ASSERT_TRUE(wait_for([&] { return hook.held >= 2; }));
    EXPECT_TRUE(c2->write<int>(0, 4));
    EXPECT_TRUE(c1->write<int>(0, 5));
    EXPECT_TRUE(c2->write<int>(0, 5));
    warned = 0;
    lost = 0;
    for (size_t i = entries_before; i < logger.entries.size(); ++i)
    {
        const auto & [level, msg] = logger.entries[i];
        if (level == sihd::util::LogLevel::warning && msg.find("almost full") != std::string::npos)
            ++warned;
        if (level == sihd::util::LogLevel::error && msg.find("are lost") != std::string::npos)
            ++lost;
    }
    EXPECT_EQ(warned, 2u);
    EXPECT_EQ(lost, 0u);

    hook.block = false;
    EXPECT_TRUE(profiler.flush());

    sihd::util::LoggerManager::rm(&logger);
}

TEST_F(TestTreeProfiler, test_trace)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");

    FakeClock clock;
    TreeProfiler profiler;
    profiler.set_clock(&clock);
    ASSERT_TRUE(profiler.observe(&root));
    profiler.set_trace(true);

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());
    ASSERT_TRUE(profiler.observe(&root));

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

TEST_F(TestTreeProfiler, test_threaded_writes)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());

    Channel *c1 = dev->get_channel("c1").value_or(nullptr);
    ASSERT_NE(c1, nullptr);

    TreeProfiler profiler;
    ASSERT_TRUE(profiler.observe(&root));

    std::atomic<bool> done = false;
    std::thread writer([&] {
        for (int i = 0; i < 100; ++i)
        {
            // the channel starts zeroed: write_on_change would skip a first zero write
            EXPECT_TRUE(c1->write<int>(0, i + 1));
            time::msleep(1);
        }
        done = true;
    });
    // concurrent reports must not disturb the notifications
    ASSERT_TRUE(wait_for([&] {
        (void)profiler.report_str();
        return done.load();
    }));
    writer.join();

    EXPECT_EQ(dev->handled, 100);
    EXPECT_EQ(profiler.channels_count(), 2u);
    EXPECT_EQ(profiler.services_count(), 1u);
}

TEST_F(TestTreeProfiler, test_concurrent_observe)
{
    Node root("root");
    for (int i = 0; i < 8; ++i)
        new Channel("c" + std::to_string(i), "int", &root);

    TreeProfiler profiler;
    int calls = 0;
    Handler<TreeProfiler::Event *> hook([&]([[maybe_unused]] TreeProfiler::Event *event) { ++calls; });
    profiler.add_observer(&hook);

    std::atomic<bool> stop = false;
    std::thread observer1([&] {
        while (stop == false)
            EXPECT_TRUE(profiler.observe(&root));
    });
    std::thread observer2([&] {
        while (stop == false)
            EXPECT_TRUE(profiler.observe(&root));
    });
    // every channel must stay usable while two threads race to observe them
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
    ASSERT_TRUE(profiler.flush());

    EXPECT_EQ(profiler.channels_count(), 8u);
    EXPECT_EQ(profiler.dropped_events(), 0u);
    EXPECT_GT(calls, 0);
}

TEST_F(TestTreeProfiler, test_stale_events_after_children_destruction)
{
    Node root("root");
    ReclaimingDevice *dev = root.add_child<ReclaimingDevice>("dev");

    TreeProfiler profiler;
    profiler.set_queue_max(0);
    BlockingObserver hook;
    profiler.add_observer(&hook);
    ASSERT_TRUE(profiler.observe(&root));

    // channels created by init() are picked up at the state change
    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());
    ASSERT_EQ(profiler.channels_count(), 1u);
    Channel *c1 = dev->get_child<Channel>("c1");
    ASSERT_NE(c1, nullptr);

    // the dispatcher is held by the first event: writes pile up in the queue
    hook.block = true;
    ASSERT_TRUE(c1->write<int>(0, 1));
    ASSERT_TRUE(wait_for([&] { return hook.held >= 1; }));
    for (int i = 0; i < 100; ++i)
        ASSERT_TRUE(c1->write<int>(0, i + 2));

    // the service stops and reclaims its channel while events are stacked:
    // the queued events become stale, they point at nothing anymore
    ASSERT_TRUE(dev->stop());
    EXPECT_EQ(dev->get_child<Channel>("c1"), nullptr);
    EXPECT_EQ(profiler.channels_count(), 1u);

    hook.block = false;
    ASSERT_TRUE(profiler.flush());

    // every queued event was delivered, stale ones included: they are
    // self-contained copies (source name, durations), no channel pointer inside
    const std::vector<TreeProfiler::Event> events = profiler.events();
    EXPECT_EQ(hook.calls, 101 * 2 + 6);
    ASSERT_EQ(events.size(), 101u * 2 + 6u);
    size_t stale = 0;
    for (const TreeProfiler::Event & event : events)
    {
        if (event.source == "root.dev.c1")
            stale += 1;
    }
    EXPECT_EQ(stale, 202u);
    EXPECT_EQ(profiler.dropped_events(), 0u);

    // the report walks the live tree: the destroyed channel is gone from it
    EXPECT_EQ(profiler.report_str().find("dev.c1"), std::string::npos);
}

TEST_F(TestTreeProfiler, test_stale_events_interleave_with_live_ones)
{
    Node root("root");
    ReclaimingDevice *a = root.add_child<ReclaimingDevice>("a");
    ReclaimingDevice *b = root.add_child<ReclaimingDevice>("b");

    TreeProfiler profiler;
    profiler.set_queue_max(0);
    BlockingObserver hook;
    profiler.add_observer(&hook);
    ASSERT_TRUE(profiler.observe(&root));
    ASSERT_TRUE(a->init());
    ASSERT_TRUE(b->init());
    ASSERT_TRUE(a->start());
    ASSERT_TRUE(b->start());
    Channel *ac1 = a->get_child<Channel>("c1");
    Channel *bc1 = b->get_child<Channel>("c1");
    ASSERT_NE(ac1, nullptr);
    ASSERT_NE(bc1, nullptr);

    hook.block = true;
    ASSERT_TRUE(ac1->write<int>(0, 1));
    ASSERT_TRUE(wait_for([&] { return hook.held >= 1; }));
    // the two sources alternate in the queue
    for (int i = 0; i < 50; ++i)
    {
        ASSERT_TRUE(ac1->write<int>(0, i + 2));
        ASSERT_TRUE(bc1->write<int>(0, i + 2));
    }
    // a stops and reclaims its channel in the middle of the backlog
    ASSERT_TRUE(a->stop());
    // the live service keeps being profiled through it
    for (int i = 0; i < 10; ++i)
        ASSERT_TRUE(bc1->write<int>(0, i + 52));

    hook.block = false;
    ASSERT_TRUE(profiler.flush());

    const std::vector<TreeProfiler::Event> events = profiler.events();
    // a.c1: 51 writes, b.c1: 60 writes, a ops: init/start/stop, b ops: init/start
    EXPECT_EQ(hook.calls, 51 * 2 + 60 * 2 + 6 + 4);
    ASSERT_EQ(events.size(), (51 + 60) * 2u + 10u);
    size_t stale = 0;
    size_t fresh = 0;
    size_t first_fresh = events.size();
    size_t last_stale = 0;
    for (size_t i = 0; i < events.size(); ++i)
    {
        if (events[i].source == "root.a.c1")
        {
            stale += 1;
            last_stale = i;
        }
        else if (events[i].source == "root.b.c1")
        {
            fresh += 1;
            if (first_fresh == events.size())
                first_fresh = i;
        }
    }
    EXPECT_EQ(stale, 102u);
    EXPECT_EQ(fresh, 120u);
    // the dead source events are interleaved with the live ones, not grouped
    EXPECT_LT(first_fresh, last_stale);
    EXPECT_EQ(profiler.dropped_events(), 0u);
    EXPECT_EQ(profiler.channels_count(), 2u);
    EXPECT_EQ(profiler.services_count(), 2u);
}

TEST_F(TestTreeProfiler, test_report_stops_chain_in_op_start)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");

    TreeProfiler profiler;
    profiler.set_queue_max(0);
    BlockingObserver hook;
    profiler.add_observer(&hook);
    ASSERT_TRUE(profiler.observe(&root));
    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(profiler.observe(&root));
    Channel *c1 = dev->get_channel("c1").value_or(nullptr);
    ASSERT_NE(c1, nullptr);

    // the report cannot drain while the dispatcher is held: its gate stays closed
    std::atomic<bool> report_done = false;
    hook.block = true;
    ASSERT_TRUE(c1->write<int>(0, 1));
    ASSERT_TRUE(wait_for([&] { return hook.held >= 1; }));
    std::thread report([&] {
        (void)profiler.report_str();
        report_done = true;
    });
    time::msleep(30);
    std::thread starter([&] { dev->start(); });
    // the notification chain is stopped inside op_start: the transition happened,
    // the start body did not run yet
    ASSERT_TRUE(wait_for([&] { return dev->device_state() == ServiceController::Starting; }));
    EXPECT_EQ(dev->handled, 0);

    hook.block = false;
    ASSERT_TRUE(wait_for([&] { return report_done.load(); }));
    ASSERT_TRUE(wait_for([&] { return dev->device_state() == ServiceController::Running; }));
    starter.join();
    report.join();

    const std::vector<TreeProfiler::Event> events = profiler.events();
    bool start_enter = false;
    bool start_exit = false;
    for (const TreeProfiler::Event & event : events)
    {
        if (event.source == "root.device" && event.what == "start")
        {
            start_enter |= event.kind == TreeProfiler::Event::enter;
            start_exit |= event.kind == TreeProfiler::Event::exit;
        }
    }
    EXPECT_TRUE(start_enter);
    EXPECT_TRUE(start_exit);
    // the profiler still works after the frozen chain: the device now watches
    // its channels and a write reaches both it and the event history
    ASSERT_TRUE(c1->write<int>(0, 2));
    EXPECT_EQ(dev->handled, 1);
    EXPECT_EQ(profiler.channels_count(), 2u);
    // drains the last event so the dispatcher is out of the stack hook
    ASSERT_TRUE(profiler.flush());
}

TEST_F(TestTreeProfiler, test_report_waits_for_in_flight_op)
{
    Node root("root");
    GatedInitDevice *dev = root.add_child<GatedInitDevice>("device");

    TreeProfiler profiler;
    profiler.set_queue_max(0);
    ASSERT_TRUE(profiler.observe(&root));

    std::thread init([&] { ASSERT_TRUE(dev->init()); });
    ASSERT_TRUE(wait_for([&] { return dev->device_state() == ServiceController::Initializing; }));

    std::atomic<bool> report_done = false;
    std::thread report([&] {
        (void)profiler.report_str();
        report_done = true;
    });
    // the report waits for the op in flight: it cannot complete while init runs
    EXPECT_FALSE(wait_for([&] { return report_done.load(); }, std::chrono::milliseconds(100)));

    dev->release_init();
    ASSERT_TRUE(wait_for([&] { return report_done.load(); }));
    init.join();
    report.join();

    // the channels created by the awaited op are in the report
    EXPECT_EQ(profiler.channels_count(), 1u);
    EXPECT_NE(profiler.report_str().find("c1"), std::string::npos);
}

TEST_F(TestTreeProfiler, test_report_from_service_op)
{
    Node root("root");
    ReportingDevice *dev = root.add_child<ReportingDevice>("device");

    TreeProfiler profiler;
    ASSERT_TRUE(profiler.observe(&root));
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

    TreeProfiler profiler;
    ASSERT_TRUE(profiler.observe(&root));
    EXPECT_TRUE(profiler.capturing());

    // the capture waits for the device to run
    profiler.start_when("device", TreeProfiler::running());
    EXPECT_FALSE(profiler.capturing());

    ASSERT_TRUE(dev->init());
    // setup and init happened before the window opened
    EXPECT_FALSE(profiler.capturing());

    ASSERT_TRUE(dev->start());
    EXPECT_TRUE(profiler.capturing());

    // the start that opened the window is captured, its enter brought along
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

    TreeProfiler profiler;
    ASSERT_TRUE(profiler.observe(&root));
    profiler.start_when("device", TreeProfiler::running());
    EXPECT_TRUE(profiler.capturing());
}

TEST_F(TestTreeProfiler, test_capture_multiple_start_conditions)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");
    ProfilingDevice *dev2 = root.add_child<ProfilingDevice>("device2");

    TreeProfiler profiler;
    ASSERT_TRUE(profiler.observe(&root));
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

    TreeProfiler profiler;
    ASSERT_TRUE(profiler.observe(&root));
    profiler.start_when("device", TreeProfiler::running());
    // the flag is born false: the stop condition is not satisfied at attach
    profiler.stop_when("flag", ChannelMatch::equal(false));
    EXPECT_FALSE(profiler.capturing());

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());
    EXPECT_TRUE(profiler.capturing());

    EXPECT_TRUE(flag->write<bool>(0, true));
    EXPECT_TRUE(profiler.capturing());
    // the write closing the window is captured
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

    TreeProfiler profiler;
    ASSERT_TRUE(profiler.observe(&root));
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

    TreeProfiler profiler;
    ASSERT_TRUE(profiler.observe(&root));
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

    TreeProfiler profiler;
    ASSERT_TRUE(profiler.observe(&root));
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

    TreeProfiler profiler;
    profiler.set_rearm(true);
    profiler.start_when("device", TreeProfiler::running());
    profiler.stop_when("flag", ChannelMatch::equal(false));
    ASSERT_TRUE(profiler.observe(&root));
    EXPECT_FALSE(profiler.capturing());

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());
    EXPECT_TRUE(profiler.capturing());

    EXPECT_TRUE(flag->write<bool>(0, true));
    EXPECT_TRUE(flag->write<bool>(0, false));
    EXPECT_FALSE(profiler.capturing());

    // the start conditions rearm: a new window opens when the device restarts
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

    TreeProfiler profiler;
    profiler.start_when("device", TreeProfiler::running());
    profiler.stop_when("flag", ChannelMatch::equal(false));
    ASSERT_TRUE(profiler.observe(&root));

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

    TreeProfiler profiler;
    profiler.start_when("device", TreeProfiler::running());
    profiler.stop_when("flag", ChannelMatch::equal(false));
    ASSERT_TRUE(profiler.observe(&root));

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());
    // the channel is born false: write_change_only only notifies on a change
    EXPECT_TRUE(flag->write<bool>(0, true));
    EXPECT_TRUE(flag->write<bool>(0, false));
    EXPECT_FALSE(profiler.capturing());

    // clear drops the stats and rearms the window: the running device
    // satisfies the start condition again
    profiler.clear();
    EXPECT_TRUE(profiler.capturing());
    EXPECT_TRUE(flag->write<bool>(0, true));
    EXPECT_TRUE(flag->write<bool>(0, false));
    EXPECT_FALSE(profiler.capturing());

    // reset drops the conditions: the capture is always on, but nothing is
    // observed anymore
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

    TreeProfiler profiler;
    profiler.start_when("device", TreeProfiler::running());
    profiler.stop_when("flag", ChannelMatch::equal(false));
    ASSERT_TRUE(profiler.observe(&root));

    EXPECT_EQ(profiler.session_duration().get(), 0);

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());
    time::msleep(1);
    const Duration running = profiler.session_duration();
    EXPECT_GT(running.get(), 0);

    flag->write<bool>(0, true);
    flag->write<bool>(0, false);
    EXPECT_FALSE(profiler.capturing());
    // a closed window freezes the duration of the session that just ended
    const Duration frozen = profiler.session_duration();
    EXPECT_GT(frozen.get(), 0);
    EXPECT_EQ(profiler.session_duration().get(), frozen.get());

    // clear rearms the window: the running device opens a new session
    profiler.clear();
    time::msleep(1);
    EXPECT_TRUE(profiler.capturing());
    EXPECT_GT(profiler.session_duration().get(), 0);

    profiler.stop_capture();
    const Duration stopped = profiler.session_duration();
    EXPECT_EQ(profiler.session_duration().get(), stopped.get());
    profiler.start_capture();
    time::msleep(1);
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

    TreeProfiler profiler;
    ASSERT_TRUE(profiler.observe(&root));

    // an observer resetting the profiler mid-notification is not called again
    // by the watcher: the notification ends unwatched, without use-after-free
    Handler<Channel *> resetter([&]([[maybe_unused]] Channel *) { profiler.reset(); });
    c1->add_observer(&resetter);

    EXPECT_TRUE(c1->write<int>(0, 1));
    EXPECT_EQ(dev->handled, 1);
    EXPECT_EQ(profiler.channels_count(), 0u);
    EXPECT_EQ(profiler.services_count(), 0u);
    EXPECT_TRUE(profiler.events().empty());

    EXPECT_TRUE(c1->write<int>(0, 2));
    EXPECT_EQ(dev->handled, 2);
}

TEST_F(TestTreeProfiler, test_threaded_event_order)
{
    Node root("root");
    new Channel("c1", "int", &root);
    new Channel("c2", "int", &root);
    Channel *c1 = root.get_child<Channel>("c1");
    Channel *c2 = root.get_child<Channel>("c2");
    ASSERT_NE(c1, nullptr);
    ASSERT_NE(c2, nullptr);

    TreeProfiler profiler;
    EventCollector collector;
    profiler.add_observer(&collector);
    ASSERT_TRUE(profiler.observe(&root));

    // two producers: the dispatched events keep their id order
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

    EXPECT_TRUE(profiler.flush());
    EXPECT_EQ(collector.calls, 200);
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

    TreeProfiler profiler;
    ASSERT_TRUE(profiler.observe(&root));

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

    // every event keeps the thread that produced it: one distinct thread per
    // producer, and the main thread differs from both
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
            EXPECT_TRUE(thread::equals(event.thread_id, c1_thread->thread_id));
        }
        else if (event.source == "root.c2")
        {
            if (c2_thread == nullptr)
                c2_thread = &event;
            EXPECT_TRUE(thread::equals(event.thread_id, c2_thread->thread_id));
        }
    }
    ASSERT_NE(c1_thread, nullptr);
    ASSERT_NE(c2_thread, nullptr);
    EXPECT_FALSE(thread::equals(c1_thread->thread_id, c2_thread->thread_id));

    EXPECT_TRUE(c1->write<int>(0, 100));
    EXPECT_FALSE(thread::equals(profiler.events().back().thread_id, c1_thread->thread_id));
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
    TreeProfiler profiler;
    profiler.set_clock(&clock);
    ASSERT_TRUE(profiler.observe(&root));

    // clear from an observer does not disturb the envelope in flight; the
    // second clear drops the first write's sample, per-observer stats survive
    LambdaObserver clearer("clearer", [&](Channel *) { profiler.clear(); });
    c1->add_observer(&clearer);

    EXPECT_TRUE(c1->write<int>(0, 1));
    EXPECT_TRUE(c1->write<int>(0, 2));
    EXPECT_EQ(dev->handled, 2);

    // two observer calls per notification cost 5ms
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

    TreeProfiler profiler;
    ASSERT_TRUE(profiler.observe(&root));

    // an observer resetting then re-observing mid-notification hands the
    // remaining hooks to fresh entries: they hold no state of the notification
    // in flight and must ignore them
    bool rebuilt = false;
    LambdaObserver rebuilder("rebuilder", [&](Channel *) {
        if (rebuilt)
            return;
        rebuilt = true;
        profiler.reset();
        profiler.observe(&root);
    });
    c1->add_observer(&rebuilder);

    EXPECT_TRUE(c1->write<int>(0, 1));
    EXPECT_EQ(dev->handled, 1);
    EXPECT_EQ(profiler.channels_count(), 2u);

    EXPECT_TRUE(c1->write<int>(0, 2));
    EXPECT_EQ(dev->handled, 2);

    // only the second write was profiled, by the entries created mid-notification
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
    TreeProfiler profiler;
    profiler.set_clock(&clock);
    ASSERT_TRUE(profiler.observe(&root));

    // a write from an observer of the same channel is rejected: a notification
    // cannot re-enter itself
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
    TreeProfiler profiler;
    profiler.set_clock(&clock);
    ASSERT_TRUE(profiler.observe(&root));

    // a write to another channel from an observer nests a whole second
    // notification inside the first one, and both are measured
    LambdaObserver forwarder("forwarder", [&](Channel *) { c2->write<int>(0, 7); });
    c1->add_observer(&forwarder);

    EXPECT_TRUE(c1->write<int>(0, 1));
    EXPECT_EQ(dev->handled, 2);

    // c1's envelope spans the nested c2 notification: begin, device, forwarder
    // (containing c2's begin + device + end), end
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

TEST_F(TestTreeProfiler, test_two_profilers)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());

    Channel *c1 = dev->get_channel("c1").value_or(nullptr);
    ASSERT_NE(c1, nullptr);

    TreeProfiler first;
    ASSERT_TRUE(first.observe(&root));

    // a second profiler cannot steal the watched channels but still observes
    // the services
    TreeProfiler second;
    ASSERT_TRUE(second.observe(&root));
    EXPECT_EQ(first.channels_count(), 2u);
    EXPECT_EQ(second.channels_count(), 0u);
    EXPECT_EQ(first.services_count(), 1u);
    EXPECT_EQ(second.services_count(), 1u);

    EXPECT_TRUE(c1->write<int>(0, 1));
    EXPECT_EQ(dev->handled, 1);
    EXPECT_EQ(first.events().size(), 2u);
    EXPECT_TRUE(second.events().empty());

    ASSERT_TRUE(dev->stop());
    ASSERT_TRUE(dev->start());
    // service operations are timed by both profilers
    EXPECT_EQ(first.events().size(), 6u);
    EXPECT_EQ(second.events().size(), 4u);
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
    TreeProfiler profiler;
    profiler.set_clock(&clock);
    ASSERT_TRUE(profiler.observe(&root));

    // an observer stopping the device mid-notification unsubscribes it from
    // the next ones; the service operation nests inside the channel
    // notification
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

    // the unsubscribed device's stats are pruned at the next notification
    const std::string expected = R"(root: sihd::util::Node
  device: test::ProfilingDevice [stopped]
    stop: n=1 total=+1ms:0us avg=+1ms:0us min=+1ms:0us max=+1ms:0us
    c1: sihd::core::Channel
      writes=2 total=+11ms:0us avg=+5ms:500us min=+3ms:0us max=+8ms:0us
      -> stopper (test::LambdaObserver): n=2 total=+5ms:0us avg=+2ms:500us min=+1ms:0us max=+4ms:0us
    c2: sihd::core::Channel
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

    int hook_calls = 0;
    Handler<TreeProfiler::Event *> hook([&]([[maybe_unused]] TreeProfiler::Event *) { ++hook_calls; });
    std::thread writer;
    {
        TreeProfiler profiler;
        profiler.add_observer(&hook);
        ASSERT_TRUE(profiler.observe(&root));
        writer = std::thread([&] {
            for (int i = 0; i < 100; ++i)
            {
                EXPECT_TRUE(c1->write<int>(0, i + 1));
                time::msleep(1);
            }
        });
        // the profiler dies while the writer thread is notifying: the
        // destructor waits for the notification to end before detaching
        time::msleep(20);
    }
    writer.join();

    EXPECT_EQ(dev->handled, 100);
    EXPECT_GT(hook_calls, 0);
}

TEST_F(TestTreeProfiler, test_reset_from_dispatch_hook)
{
    Node root("root");
    ProfilingDevice *dev = root.add_child<ProfilingDevice>("device");

    ASSERT_TRUE(dev->init());
    ASSERT_TRUE(dev->start());

    Channel *c1 = dev->get_channel("c1").value_or(nullptr);
    ASSERT_NE(c1, nullptr);

    TreeProfiler profiler;
    // the hook resets the profiler from the dispatch thread: the channels are
    // detached and every later write goes unwatched
    Handler<TreeProfiler::Event *> hook([&]([[maybe_unused]] TreeProfiler::Event *) { profiler.reset(); });
    profiler.add_observer(&hook);
    ASSERT_TRUE(profiler.observe(&root));

    EXPECT_TRUE(c1->write<int>(0, 1));
    ASSERT_TRUE(profiler.flush());
    EXPECT_EQ(profiler.channels_count(), 0u);
    EXPECT_EQ(profiler.services_count(), 0u);
    EXPECT_TRUE(profiler.events().empty());

    EXPECT_TRUE(c1->write<int>(0, 2));
    EXPECT_EQ(dev->handled, 2);
}

} // namespace test

#include <atomic>
#include <thread>

#include <gtest/gtest.h>

#include <sihd/util/ABlockingService.hpp>
#include <sihd/util/AService.hpp>
#include <sihd/util/AStepWorkerService.hpp>
#include <sihd/util/AThreadedService.hpp>
#include <sihd/util/AWorkerService.hpp>
#include <sihd/util/BlockingServiceController.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/ServiceController.hpp>
#include <sihd/util/Synchronizer.hpp>
#include <sihd/util/ThreadedServiceController.hpp>
#include <sihd/util/Waitable.hpp>
#include <sihd/util/profiling.hpp>
#include <sihd/util/time.hpp>

#include "wait/wait_for.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;

class FakeThreadedService: public AThreadedService
{
    public:
        FakeThreadedService(): AThreadedService("FakeThreadedService") { this->set_service_nb_thread(1); };
        ~FakeThreadedService() { this->stop(); }

        bool on_start() override
        {
            auto b = std::make_shared<Synchronizer>(3);
            thread1 = std::thread([b] { b->sync(); });
            thread2 = std::thread([b] { b->sync(); });
            thread3 = std::thread([b, this] {
                b->sync();
                started = true;
                this->notify_service_thread_started();
            });
            return true;
        }

        bool on_stop() override
        {
            for (std::thread *worker : {&thread1, &thread2, &thread3})
                if (worker->joinable())
                    worker->join();
            started = false;
            return true;
        }

        bool is_running() const override { return started; }

        bool started = false;
        std::thread thread1;
        std::thread thread2;
        std::thread thread3;
};

class FakeBlockingService: public ABlockingService
{
    public:
        ~FakeBlockingService() { this->stop(); }

        bool on_start() override
        {
            this->service_set_ready();
            if (stop_from_own_thread)
                this->stop();
            _waitable.wait([this] { return _stop_requested || ends_by_itself; });
            _stop_requested = false;
            return true;
        }

        bool on_stop() override
        {
            {
                auto lock = _waitable.guard();
                _stop_requested = true;
            }
            _waitable.notify_all();
            return true;
        }

        bool ends_by_itself = false;
        bool stop_from_own_thread = false;

    private:
        Waitable _waitable;
        bool _stop_requested = false;
};

class FakeService: public AService
{
    public:
        int n_setup = 0;
        int n_init = 0;
        int n_start = 0;
        int n_stop = 0;
        int n_reset = 0;
        bool fail_init = false;
        bool fail_start = false;
        bool fail_stop = false;

        bool is_running() const { return n_start > 0; }

        bool do_setup()
        {
            ++n_setup;
            return true;
        }
        bool do_init()
        {
            ++n_init;
            return !fail_init;
        }
        bool do_start()
        {
            ++n_start;
            return !fail_start;
        }
        bool do_stop()
        {
            ++n_stop;
            return !fail_stop;
        }
        bool do_reset()
        {
            ++n_reset;
            return true;
        }
};

class FakeWorkerService: public AWorkerService
{
    public:
        FakeWorkerService(): AWorkerService("FakeWorkerService") {}
        ~FakeWorkerService() { this->stop(); }

        bool on_work_start() override
        {
            work_started = true;
            while (stop_requested == false)
                time::msleep(1);
            return true;
        }
        bool on_work_stop() override
        {
            work_stopped = true;
            return true;
        }

        std::atomic<bool> work_started {false};
        std::atomic<bool> work_stopped {false};
};

class FakeStepWorkerService: public AStepWorkerService
{
    public:
        FakeStepWorkerService(): AStepWorkerService("FakeStepWorkerService") {}
        ~FakeStepWorkerService() { this->stop(); }

        bool on_work_setup() override
        {
            work_setup_done = true;
            return true;
        }
        bool on_work_start() override
        {
            ++work_step_count;
            return true;
        }
        bool on_work_stop() override
        {
            work_stopped = true;
            return true;
        }
        bool on_work_teardown() override
        {
            work_teardown_done = true;
            return true;
        }

        std::atomic<bool> work_setup_done {false};
        std::atomic<bool> work_stopped {false};
        std::atomic<bool> work_teardown_done {false};
        std::atomic<int> work_step_count {0};
};

class FakeServiceWithController: public FakeService
{
    public:
        ServiceController ctrl;
        IServiceController *service_ctrl() override { return &ctrl; };
};

class TestService: public ::testing::Test,
                   public IHandler<AService *>,
                   public IHandler<ServiceController *>
{
    protected:
        TestService() { sihd::util::LoggerManager::stream(); }

        virtual ~TestService() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() { obs_changed = 0; }

        virtual void TearDown() {}

        virtual void handle(AService *service)
        {
            ++obs_changed;
            (void)service;
            SIHD_TRACE("Service changed state");
        }

        virtual void handle(ServiceController *ctrl)
        {
            ++obs_changed;
            auto machine = ctrl->statemachine;
            SIHD_LOG(debug, "Service state: {}", machine.state_name(machine.state()));
        }

        int obs_changed;
};

TEST_F(TestService, test_service_ctrl)
{
    FakeServiceWithController service;
    EXPECT_NE(service.service_ctrl(), nullptr);
    service.ctrl.add_observer(this);

    // cannot do anything else than setup (except reset)
    EXPECT_FALSE(service.init());
    EXPECT_FALSE(service.start());
    EXPECT_FALSE(service.stop());
    EXPECT_EQ(service.n_setup, 0);
    EXPECT_EQ(service.n_init, 0);
    EXPECT_EQ(service.n_start, 0);
    EXPECT_EQ(service.n_stop, 0);
    EXPECT_EQ(obs_changed, 0);

    // setup
    EXPECT_TRUE(service.setup());
    EXPECT_EQ(service.n_setup, 1);
    // none -> CONFIGURING -> CONFIGURED
    EXPECT_EQ(obs_changed, 2);

    // cannot do anything else than init (except reset)
    EXPECT_FALSE(service.start());
    EXPECT_FALSE(service.stop());
    EXPECT_EQ(service.n_init, 0);
    EXPECT_EQ(service.n_start, 0);
    EXPECT_EQ(service.n_stop, 0);
    EXPECT_FALSE(service.setup());
    EXPECT_EQ(obs_changed, 2);

    // init
    EXPECT_TRUE(service.init());
    EXPECT_EQ(service.n_setup, 1);
    EXPECT_EQ(service.n_init, 1);
    // CONFIGURED -> initializing -> stopped
    EXPECT_EQ(obs_changed, 4);

    // cannot do anything else than start (except reset)
    EXPECT_TRUE(service.stop());
    EXPECT_EQ(service.n_start, 0);
    EXPECT_EQ(service.n_stop, 0);
    EXPECT_FALSE(service.setup());
    EXPECT_FALSE(service.init());
    EXPECT_EQ(obs_changed, 4);

    // start
    EXPECT_FALSE(service.is_running());
    EXPECT_TRUE(service.start());
    EXPECT_TRUE(service.is_running());
    EXPECT_EQ(service.n_setup, 1);
    EXPECT_EQ(service.n_init, 1);
    EXPECT_EQ(service.n_start, 1);
    EXPECT_EQ(service.n_stop, 0);
    // stopped -> starting -> running
    EXPECT_EQ(obs_changed, 6);

    // cannot do previous states
    EXPECT_FALSE(service.setup());
    EXPECT_FALSE(service.init());

    // stop
    EXPECT_TRUE(service.stop());
    EXPECT_EQ(service.n_stop, 1);
    // running -> stopping -> stopped
    EXPECT_EQ(obs_changed, 8);
    // start
    EXPECT_TRUE(service.start());
    EXPECT_EQ(service.n_start, 2);
    // stopped -> starting -> running
    EXPECT_EQ(obs_changed, 10);

    // cannot reset from start
    EXPECT_FALSE(service.reset());

    // stop
    EXPECT_TRUE(service.stop());
    EXPECT_EQ(service.n_stop, 2);
    // running -> stopping -> stopped
    EXPECT_EQ(obs_changed, 12);

    // reset
    EXPECT_TRUE(service.reset());
    EXPECT_EQ(service.n_reset, 1);
    // stopped -> resetting -> none
    EXPECT_EQ(obs_changed, 14);

    // cannot re-reset
    EXPECT_FALSE(service.reset());
    EXPECT_EQ(service.n_reset, 1);

    // setup -> rest
    EXPECT_TRUE(service.setup());
    EXPECT_TRUE(service.reset());

    // init -> reset
    EXPECT_TRUE(service.setup());
    EXPECT_TRUE(service.init());
    EXPECT_TRUE(service.reset());
}

TEST_F(TestService, test_service_ctrl_failures)
{
    FakeServiceWithController init_failed;
    init_failed.fail_init = true;
    FakeServiceWithController start_failed;
    start_failed.fail_start = true;
    FakeServiceWithController stop_failed;
    stop_failed.fail_stop = true;

    EXPECT_TRUE(init_failed.setup());
    EXPECT_FALSE(init_failed.init());
    EXPECT_EQ(init_failed.ctrl.state(), ServiceController::Failure);
    EXPECT_FALSE(init_failed.start());
    EXPECT_FALSE(init_failed.stop());
    EXPECT_EQ(init_failed.n_start, 0);
    EXPECT_EQ(init_failed.n_stop, 0);

    EXPECT_TRUE(start_failed.setup());
    EXPECT_TRUE(start_failed.init());
    EXPECT_FALSE(start_failed.start());
    EXPECT_EQ(start_failed.ctrl.state(), ServiceController::StartFailure);
    EXPECT_FALSE(start_failed.stop());
    start_failed.fail_start = false;
    EXPECT_TRUE(start_failed.start());
    EXPECT_EQ(start_failed.n_start, 2);
    EXPECT_EQ(start_failed.n_stop, 0);

    EXPECT_TRUE(stop_failed.setup());
    EXPECT_TRUE(stop_failed.init());
    EXPECT_TRUE(stop_failed.start());
    EXPECT_FALSE(stop_failed.stop());
    EXPECT_FALSE(stop_failed.start());
    EXPECT_EQ(stop_failed.ctrl.state(), ServiceController::StopFailure);
    stop_failed.fail_stop = false;
    EXPECT_TRUE(stop_failed.stop());
    EXPECT_TRUE(stop_failed.stop());
    EXPECT_EQ(stop_failed.n_stop, 2);
    EXPECT_EQ(stop_failed.ctrl.state(), ServiceController::Stopped);
}

TEST_F(TestService, test_service_controllers_failures)
{
    BlockingServiceController blocking;
    ThreadedServiceController threaded;

    EXPECT_EQ(blocking.op_start(AService::Init), AService::Transition::proceed);
    EXPECT_TRUE(blocking.op_end(AService::Init, false));
    EXPECT_EQ(blocking.op_start(AService::Start), AService::Transition::refused);
    EXPECT_EQ(blocking.op_start(AService::Stop), AService::Transition::refused);

    EXPECT_TRUE(blocking.op_end(AService::Init, true));
    EXPECT_EQ(blocking.op_start(AService::Start), AService::Transition::proceed);
    EXPECT_EQ(blocking.op_start(AService::Start), AService::Transition::refused);

    EXPECT_EQ(blocking.op_start(AService::Stop), AService::Transition::proceed);
    EXPECT_TRUE(blocking.op_end(AService::Stop, false));
    EXPECT_EQ(blocking.op_start(AService::Start), AService::Transition::refused);
    EXPECT_EQ(blocking.op_start(AService::Stop), AService::Transition::proceed);
    EXPECT_TRUE(blocking.op_end(AService::Stop, true));

    EXPECT_TRUE(blocking.op_end(AService::Start, false));
    EXPECT_EQ(blocking.op_start(AService::Stop), AService::Transition::refused);
    EXPECT_EQ(blocking.op_start(AService::Start), AService::Transition::proceed);

    EXPECT_EQ(threaded.op_start(AService::Init), AService::Transition::proceed);
    EXPECT_TRUE(threaded.op_end(AService::Init, false));
    EXPECT_EQ(threaded.op_start(AService::Start), AService::Transition::refused);
    EXPECT_EQ(threaded.op_start(AService::Stop), AService::Transition::refused);

    EXPECT_TRUE(threaded.op_end(AService::Init, true));
    EXPECT_EQ(threaded.op_start(AService::Start), AService::Transition::proceed);
    EXPECT_EQ(threaded.op_start(AService::Stop), AService::Transition::refused);
    EXPECT_TRUE(threaded.op_end(AService::Start, false));
    EXPECT_EQ(threaded.op_start(AService::Stop), AService::Transition::refused);

    EXPECT_EQ(threaded.op_start(AService::Start), AService::Transition::proceed);
    EXPECT_TRUE(threaded.op_end(AService::Start, true));
    EXPECT_EQ(threaded.op_start(AService::Stop), AService::Transition::proceed);
    EXPECT_EQ(threaded.op_start(AService::Stop), AService::Transition::done);
    EXPECT_TRUE(threaded.op_end(AService::Stop, false));
    EXPECT_EQ(threaded.op_start(AService::Start), AService::Transition::refused);
    EXPECT_EQ(threaded.op_start(AService::Stop), AService::Transition::proceed);
}

TEST_F(TestService, test_service_normal_impl)
{
    FakeService service;
    service.service_state().add_observer(this);
    EXPECT_TRUE(service.setup());
    EXPECT_TRUE(service.init());
    EXPECT_TRUE(service.start());
    EXPECT_TRUE(service.is_running());
    EXPECT_TRUE(service.stop());
    EXPECT_TRUE(service.reset());

    EXPECT_EQ(service.n_setup, 1);
    EXPECT_EQ(service.n_init, 1);
    EXPECT_EQ(service.n_start, 1);
    EXPECT_EQ(service.n_stop, 1);
    EXPECT_EQ(service.n_reset, 1);

    EXPECT_TRUE(service.start());
    EXPECT_EQ(service.n_start, 2);

    EXPECT_EQ(service.service_ctrl(), nullptr);

    EXPECT_EQ(obs_changed, 6);
}

TEST_F(TestService, test_service_blocking)
{
    FakeBlockingService service;
    service.set_service_wait_stop(true);
    EXPECT_TRUE(service.stop());
    EXPECT_FALSE(service.is_running());

    for (int run = 0; run < 2; ++run)
    {
        bool started = false;
        std::jthread runner([&service, &started] { started = service.start(); });

        EXPECT_TRUE(service.wait_ready(Duration(time::ms(500))));
        EXPECT_TRUE(service.is_ready());
        EXPECT_TRUE(service.is_running());
        EXPECT_FALSE(service.start());

        EXPECT_TRUE(service.stop());
        runner.join();

        EXPECT_TRUE(started);
        EXPECT_FALSE(service.is_running());
        EXPECT_FALSE(service.is_ready());
        EXPECT_TRUE(service.stop());
    }
}

TEST_F(TestService, test_service_blocking_ends_by_itself)
{
    FakeBlockingService service;
    service.ends_by_itself = true;

    const bool first = service.start();
    const bool second = service.start();

    EXPECT_TRUE(first);
    EXPECT_TRUE(second);
    EXPECT_FALSE(service.is_running());
    EXPECT_TRUE(service.stop());
}

TEST_F(TestService, test_service_blocking_stop_from_own_thread)
{
    FakeBlockingService service;
    service.set_service_wait_stop(true);
    service.stop_from_own_thread = true;

    const bool started = service.start();

    EXPECT_TRUE(started);
    EXPECT_FALSE(service.is_running());
    EXPECT_TRUE(service.stop());
}

TEST_F(TestService, test_service_threaded)
{
    std::unique_ptr<AService> service_ptr = std::make_unique<FakeThreadedService>();

    dynamic_cast<AThreadedService *>(service_ptr.get())->set_start_synchronised(true);

    EXPECT_TRUE(service_ptr->stop());

    EXPECT_TRUE(service_ptr->start());
    EXPECT_FALSE(service_ptr->start());
    EXPECT_TRUE(service_ptr->is_running());

    EXPECT_TRUE(service_ptr->stop());
    EXPECT_FALSE(service_ptr->is_running());
    EXPECT_TRUE(service_ptr->stop());

    EXPECT_TRUE(service_ptr->reset());
}

TEST_F(TestService, test_service_worker)
{
    FakeWorkerService service;
    service.set_start_synchronised(true);

    EXPECT_TRUE(service.stop());
    EXPECT_TRUE(service.start());
    EXPECT_TRUE(service.is_running());
    EXPECT_TRUE(wait_for([&] { return service.work_started.load(); }));
    EXPECT_FALSE(service.start());

    EXPECT_TRUE(service.stop());
    EXPECT_FALSE(service.is_running());
    EXPECT_TRUE(service.work_stopped.load());
    EXPECT_TRUE(service.stop());
    EXPECT_TRUE(service.reset());
}

TEST_F(TestService, test_service_step_worker)
{
    FakeStepWorkerService service;
    EXPECT_TRUE(service.set_step_frequency(200));
    service.set_start_synchronised(true);

    EXPECT_TRUE(service.stop());
    EXPECT_TRUE(service.start());
    EXPECT_TRUE(service.is_running());
    EXPECT_TRUE(service.work_setup_done.load());
    EXPECT_TRUE(wait_for([&] { return service.work_step_count.load() > 0; }));

    const int step_count = service.work_step_count.load();
    EXPECT_TRUE(wait_for([&] { return service.work_step_count.load() > step_count; }));

    EXPECT_TRUE(service.stop());
    EXPECT_FALSE(service.is_running());
    EXPECT_TRUE(service.work_stopped.load());
    EXPECT_TRUE(service.work_teardown_done.load());
    EXPECT_TRUE(service.reset());
}

} // namespace test

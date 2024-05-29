#include <gtest/gtest.h>
#include <sihd/util/AService.hpp>
#include <sihd/util/BlockingService.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/ServiceController.hpp>
#include <sihd/util/ThreadedService.hpp>

#include <sihd/util/profiling.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;

class FakeThreadedService: public ThreadedService
{
    public:
        FakeThreadedService() { this->set_service_nb_thread(1); };
        ~FakeThreadedService() { this->stop(); }

        virtual bool on_start() override
        {
            thread = std::thread([this] {
                this->notify_service_thread_started();
                started = true;
            });
            return true;
        }

        virtual bool on_stop() override
        {
            started = false;
            if (thread.joinable())
                thread.join();
            return true;
        }

        virtual bool is_running() const override { return started; }

        bool started = false;
        std::thread thread;
};

class FakeBlockingService: public BlockingService
{
    public:
        ~FakeBlockingService()
        {
            this->stop();
            this->service_wait_stop();
        }

        virtual bool on_start() override
        {
            started = true;
            return true;
        }
        virtual bool on_stop() override
        {
            started = false;
            return true;
        }
        virtual bool is_running() const override { return started; }

        bool started = false;
};

class FakeService: public AService
{
    public:
        int n_setup = 0;
        int n_init = 0;
        int n_start = 0;
        int n_stop = 0;
        int n_reset = 0;

        virtual bool is_running() const { return n_start > 0; }

        virtual bool do_setup()
        {
            ++n_setup;
            return true;
        }
        virtual bool do_init()
        {
            ++n_init;
            return true;
        }
        virtual bool do_start()
        {
            ++n_start;
            return true;
        }
        virtual bool do_stop()
        {
            ++n_stop;
            return true;
        }
        virtual bool do_reset()
        {
            ++n_reset;
            return true;
        }
};

class FakeServiceWithController: public FakeService
{
    public:
        ServiceController ctrl;
        virtual IServiceController *service_ctrl() override { return &ctrl; };
};

class TestService: public ::testing::Test,
                   public IHandler<AService *>,
                   public IHandler<ServiceController *>
{
    protected:
        TestService() { sihd::util::LoggerManager::basic(); }

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
    EXPECT_FALSE(service.stop());
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

    EXPECT_FALSE(service.stop());
    EXPECT_FALSE(service.is_running());

    EXPECT_TRUE(service.start());
    EXPECT_TRUE(service.is_running());
    EXPECT_FALSE(service.start());

    EXPECT_TRUE(service.stop());
    EXPECT_FALSE(service.is_running());
    EXPECT_FALSE(service.stop());

    EXPECT_TRUE(service.start());
    EXPECT_TRUE(service.is_running());
    EXPECT_FALSE(service.start());

    service.service_ctrl();
    service.service_state();
}

TEST_F(TestService, test_service_threaded)
{
    FakeThreadedService service;

    service.set_start_synchronised(true);

    EXPECT_FALSE(service.stop());

    EXPECT_TRUE(service.start());
    EXPECT_FALSE(service.start());
    EXPECT_TRUE(service.is_running());

    EXPECT_TRUE(service.stop());
    EXPECT_FALSE(service.is_running());
    EXPECT_FALSE(service.stop());

    EXPECT_TRUE(service.reset());
}

} // namespace test

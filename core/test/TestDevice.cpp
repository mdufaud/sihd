#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/core/Device.hpp>

namespace test
{
    LOGGER;
    using namespace sihd::core;
    class TestDevice:   public ::testing::Test
    {
        protected:
            TestDevice()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestDevice()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }
    };

    class SomeDevice: public Device
    {
        public:
            SomeDevice(const std::string & name, Node *parent = nullptr): Device(name, parent)
            {
                _running = false;
            }

            ~SomeDevice() {}

            bool    is_running() const
            {
                return _running;
            }

            void    observable_changed(Channel *c)
            {
                (void)c;
            }

        protected:
            bool    on_init() override
            {
                bool ret = Device::on_init();
                this->add_channel("c1", "int");
                return ret;
            }

            bool    on_start()
            {
                bool ret = Device::on_start();
                _running = ret;
                return ret;
            }

            bool    on_stop()
            {
                bool ret = Device::on_stop();
                _running = false;
                return ret;
            }

        private:
            bool _running;
    };

    TEST_F(TestDevice, test_device)
    {
        SomeDevice dev("device");

        EXPECT_EQ(dev.get_channel("c1"), nullptr);
        EXPECT_TRUE(dev.init());
        EXPECT_NE(dev.get_channel("c1"), nullptr);
        EXPECT_TRUE(dev.start());
        EXPECT_NE(dev.get_channel("c1"), nullptr);
        EXPECT_TRUE(dev.stop());
        EXPECT_NE(dev.get_channel("c1"), nullptr);
    }
}

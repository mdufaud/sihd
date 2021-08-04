#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Callback.hpp>

namespace test
{
    LOGGER;
    using namespace sihd::util;
    class TestCallback:   public ::testing::Test
    {
        protected:
            TestCallback()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestCallback()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }

        public:
            bool    test_cb(std::string val)
            {
                return val != "hello world";
            }

            void    test_cb_no_args()
            {
                ++counter;
            }

            int counter = 0;
    };

    TEST_F(TestCallback, test_callback_class)
    {
        CallbackManager cbm;

        cbm.set<TestCallback, bool, std::string>("test_class", this, &TestCallback::test_cb);
        bool ret = cbm.call<bool, std::string>("test_class", "hello");
        EXPECT_TRUE(ret);
        ret = cbm.call<bool, std::string>("test_class", "hello world");
        EXPECT_FALSE(ret);

        cbm.set<TestCallback>("test_class_no_args", this, &TestCallback::test_cb_no_args);
        EXPECT_EQ(this->counter, 0);
        EXPECT_NO_THROW(cbm.call("test_class_no_args"));
        EXPECT_EQ(this->counter, 1);
    }

    TEST_F(TestCallback, test_callback)
    {
        CallbackManager cbm;

        bool called = false;
        cbm.set("test_noret", [&called] () { called = true; });
        EXPECT_NO_THROW(cbm.call("test_noret"));
        EXPECT_EQ(called, true);

        EXPECT_THROW(cbm.call("no_such_key"), std::out_of_range);

        //TODO
        //cbm.set<bool>("test_noarg", [] () { return true; });
        //EXPECT_NO_THROW(EXPECT_EQ(cbm.call<bool>("test_noarg"), true));

        //cbm.set<bool, int>("test_1arg", [] (int i) { return i % 2 == 0; });
        //EXPECT_NO_THROW(EXPECT_EQ(cbm.call<bool>("test_1arg", 2), true));
        //EXPECT_NO_THROW(EXPECT_EQ(cbm.call<bool>("test_1arg", 3), false));
    }
}

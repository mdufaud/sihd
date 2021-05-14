#include <gtest/gtest.h>
#include <iostream>
#include <sihd/core/thread.hpp>

namespace test
{
    using namespace sihd::core;
    class TestThread:   public ::testing::Test
    {
        protected:
            TestThread() {}
            virtual ~TestThread() {}

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }

            std::thread::id  main_id;
            std::thread::id  other_id;
            std::string      other_name;

        public:
            void    test()
            {
                this->other_id = thread::id();
                thread::set_name("another-thread");
                this->other_name = thread::get_name();
            }
    };

    TEST_F(TestThread, test_thread_name)
    {
        this->main_id = thread::id();
        EXPECT_EQ(thread::get_name(), "main");
        std::thread t(&TestThread::test, this);
        t.join();
        EXPECT_EQ(thread::get_name(), "main");
        EXPECT_EQ(this->other_name, "another-thread");
        EXPECT_NE(this->other_id, this->main_id);
        EXPECT_EQ(this->main_id, thread::id());
    }
}

#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Thread.hpp>

namespace test
{
    using namespace sihd::util;
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

            pthread_t main_id;
            pthread_t other_id;
            std::string other_name;

        public:
            void test()
            {
                this->other_id = Thread::id();
                Thread::set_name("another-thread");
                this->other_name = Thread::get_name();
            }
    };

    TEST_F(TestThread, test_thread_name)
    {
        this->main_id = Thread::id();
        EXPECT_EQ(Thread::get_name(), "main");
        std::thread t(&TestThread::test, this);
        t.join();
        EXPECT_EQ(Thread::get_name(), "main");
        EXPECT_EQ(this->other_name, "another-thread");
        EXPECT_FALSE(Thread::equals(this->other_id, this->main_id));
        EXPECT_TRUE(Thread::equals(this->main_id,  Thread::id()));
    }
}

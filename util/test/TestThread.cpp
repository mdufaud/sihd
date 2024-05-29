#include <gtest/gtest.h>
#include <sihd/util/thread.hpp>

namespace test
{
using namespace sihd::util;
class TestThread: public ::testing::Test
{
    protected:
        TestThread() {}
        virtual ~TestThread() {}

        virtual void SetUp() {}

        virtual void TearDown() {}

        pthread_t main_id;
        pthread_t other_id;
        std::string other_name;

    public:
        void test()
        {
            this->other_id = thread::id();
            thread::set_name("another-thread");
            this->other_name = thread::name();
        }
};

TEST_F(TestThread, test_thread_name)
{
    this->main_id = thread::id();
    EXPECT_EQ(thread::name(), "main");
    std::thread t(&TestThread::test, this);
    t.join();
    EXPECT_EQ(thread::name(), "main");
    EXPECT_EQ(this->other_name, "another-thread");
    EXPECT_FALSE(thread::equals(this->other_id, this->main_id));
    EXPECT_TRUE(thread::equals(this->main_id, thread::id()));
}
} // namespace test

#include <gtest/gtest.h>

#include <sihd/util/thread.hpp>

namespace test
{
using namespace sihd::util;
class TestThread: public ::testing::Test
{
    protected:
        TestThread() = default;
        virtual ~TestThread() = default;

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
    std::string current_thread_name = thread::name();
    EXPECT_FALSE(current_thread_name.empty());
    std::thread t(&TestThread::test, this);
    t.join();
    EXPECT_EQ(thread::name(), current_thread_name);
    EXPECT_EQ(this->other_name, "another-thread");
    EXPECT_FALSE(thread::equals(this->other_id, this->main_id));
    EXPECT_TRUE(thread::equals(this->main_id, thread::id()));
}

TEST_F(TestThread, test_thread_main_id)
{
    // thread::main() captures the program entry thread — must equal the test runner thread ID
    const pthread_t main_id = thread::main();
    EXPECT_TRUE(thread::equals(main_id, thread::id()));

    pthread_t child_id;
    std::thread t([&child_id] { child_id = thread::id(); });
    t.join();

    EXPECT_FALSE(thread::equals(main_id, child_id));
}

TEST_F(TestThread, test_thread_id_str)
{
    const std::string s = thread::id_str();
    EXPECT_FALSE(s.empty());

    std::string child_str;
    std::thread t([&child_str] { child_str = thread::id_str(); });
    t.join();

    EXPECT_FALSE(child_str.empty());
    EXPECT_NE(s, child_str);
}
} // namespace test

#include <gtest/gtest.h>

#include <sihd/util/AtExit.hpp>
#include <sihd/util/IRunnable.hpp>

namespace test
{
using namespace sihd::util;

class RunnableFlag: public IRunnable
{
    public:
        bool & _flag;
        RunnableFlag(bool & f): _flag(f) {}
        bool run() override
        {
            _flag = true;
            return true;
        }
};

class TestAtExit: public ::testing::Test
{
    protected:
        TestAtExit() = default;
        virtual ~TestAtExit() = default;
        virtual void SetUp() { atexit::clear_handlers(); }
        virtual void TearDown() { atexit::clear_handlers(); }
};

TEST_F(TestAtExit, test_atexit_add_remove)
{
    bool called = false;
    auto *handler = new RunnableFlag(called);

    atexit::add_handler(handler);
    atexit::remove_handler(handler);

    atexit::exit_callback();
    EXPECT_FALSE(called);

    delete handler;
}

TEST_F(TestAtExit, test_atexit_callback_fires)
{
    static bool fired = false;
    fired = false;
    auto *handler = new RunnableFlag(fired);

    EXPECT_TRUE(atexit::install());
    atexit::add_handler(handler);
    atexit::exit_callback();

    EXPECT_TRUE(fired);
}

} // namespace test

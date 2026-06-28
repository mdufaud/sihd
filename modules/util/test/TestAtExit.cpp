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

TEST_F(TestAtExit, test_atexit_callback_order)
{
    std::vector<int> call_order;

    struct OrderHandler: public IRunnable
    {
            std::vector<int> & _order;
            int _id;
            OrderHandler(std::vector<int> & o, int id): _order(o), _id(id) {}
            bool run() override
            {
                _order.push_back(_id);
                return true;
            }
    };

    atexit::install();
    atexit::add_handler(new OrderHandler(call_order, 1));
    atexit::add_handler(new OrderHandler(call_order, 2));
    atexit::add_handler(new OrderHandler(call_order, 3));

    atexit::exit_callback();

    ASSERT_EQ(call_order.size(), 3u);
    EXPECT_EQ(call_order[0], 1);
    EXPECT_EQ(call_order[1], 2);
    EXPECT_EQ(call_order[2], 3);
}

TEST_F(TestAtExit, test_atexit_removed_not_fired)
{
    bool fired_a = false;
    bool fired_b = false;

    auto *a = new RunnableFlag(fired_a);
    auto *b = new RunnableFlag(fired_b);

    atexit::install();
    atexit::add_handler(a);
    atexit::add_handler(b);
    atexit::remove_handler(a);
    delete a;

    atexit::exit_callback();

    EXPECT_FALSE(fired_a);
    EXPECT_TRUE(fired_b);
    // b was deleted by exit_callback()
}

} // namespace test

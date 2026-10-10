#include <gtest/gtest.h>

#include <sihd/util/Callback.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;

int non_member_counter;

void void_no_args()
{
    ++non_member_counter;
}

void void_args(int a)
{
    non_member_counter += a;
}

int ret_no_args()
{
    return 3;
}

int ret_args(int a, int b)
{
    return a + b;
}

class TestCallback: public ::testing::Test
{
    protected:
        TestCallback() { sihd::util::LoggerManager::stream(); }

        virtual ~TestCallback() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() { non_member_counter = 0; }

        virtual void TearDown() {}

    public:
        void m_void_no_args() { ++counter; }

        void m_void_args(int a) { counter *= a; }

        std::string m_ret_no_args() { return "hello"; }

        bool m_ret_args(std::string val) { return val != "hello world"; }

        int counter = 0;
};

TEST_F(TestCallback, test_callback_non_member)
{
    CallbackManager cbm;

    cbm.set("test_void_no_args", &void_no_args);
    cbm.set("test_void_args", &void_args);
    cbm.set("test_ret_no_args", &ret_no_args);
    cbm.set("test_ret_args", &ret_args);

    EXPECT_EQ(non_member_counter, 0);
    cbm.call("test_void_no_args");
    EXPECT_EQ(non_member_counter, 1);
    cbm.call("test_void_args", 3);
    EXPECT_EQ(non_member_counter, 4);

    EXPECT_EQ(cbm.call<int>("test_ret_no_args"), 3);

    // Need to pass the whole signature here otherwise the call is ambiguous
    // because all the types in the signature are the same.
    int res = cbm.call<int, int, int>("test_ret_args", 3, 4);
    EXPECT_EQ(res, 7);

    EXPECT_THROW(cbm.call("no_such_key"), std::out_of_range);
    EXPECT_THROW(cbm.call("test_void_args", 2.2, "test"), std::invalid_argument);
}

TEST_F(TestCallback, test_callback_member)
{
    CallbackManager cbm;

    cbm.set<TestCallback>("test_void_no_args", this, &TestCallback::m_void_no_args);
    cbm.set<TestCallback>("test_void_args", this, &TestCallback::m_void_args);
    cbm.set<TestCallback>("test_ret_no_args", this, &TestCallback::m_ret_no_args);
    cbm.set<TestCallback>("test_ret_args", this, &TestCallback::m_ret_args);

    EXPECT_EQ(counter, 0);
    cbm.call("test_void_no_args");
    EXPECT_EQ(counter, 1);
    cbm.call("test_void_args", 3);
    EXPECT_EQ(counter, 3);

    EXPECT_EQ(cbm.call<std::string>("test_ret_no_args"), "hello");
    EXPECT_EQ(cbm.call<bool>("test_ret_args", std::string("hello world")), false);
    EXPECT_EQ(cbm.call<bool>("test_ret_args", std::string("test")), true);
}

TEST_F(TestCallback, test_callback_lambda)
{
    CallbackManager cbm;

    bool called = false;
    int value = 0;

    // Always need to give the entire signature for lambdas.
    cbm.set<void>("test_void_no_args", [&called]() { called = true; });
    cbm.set<void, int>("test_void_args", [&value](int a) { value = a; });
    cbm.set<std::string>("test_ret_no_args", []() { return "test_ok"; });
    cbm.set<int, int, int>("test_ret_args", [](int a, int b) { return a * b; });

    EXPECT_NO_THROW(cbm.call("test_void_no_args"));
    EXPECT_EQ(called, true);

    EXPECT_EQ(value, 0);
    cbm.call("test_void_args", 4);
    EXPECT_EQ(value, 4);

    EXPECT_EQ(cbm.call<std::string>("test_ret_no_args"), "test_ok");

    int ret = cbm.call<int, int, int>("test_ret_args", 5, 6);
    EXPECT_EQ(ret, 30);

    EXPECT_THROW(cbm.call("no_such_key"), std::out_of_range);

    EXPECT_TRUE(cbm.exists("test_void_args"));
    cbm.remove("test_void_args");
    EXPECT_FALSE(cbm.exists("test_void_args"));

    cbm.remove("no_such_key");
}

TEST_F(TestCallback, test_callback_check_call_type)
{
    CallbackManager cbm;

    cbm.set<void, int>("test_void_args", [](int) {});
    cbm.set<int>("test_ret", []() { return 42; });

    EXPECT_TRUE((cbm.check_call_type<void, int>("test_void_args")));
    EXPECT_FALSE(cbm.check_call_type<void>("test_void_args"));
    EXPECT_FALSE((cbm.check_call_type<int, int>("test_void_args")));
    EXPECT_TRUE(cbm.check_call_type<int>("test_ret"));
    EXPECT_FALSE(cbm.check_call_type<int>("no_such_key"));
}

TEST(TestHandler, test_handler_method)
{
    Handler<int> handler;
    EXPECT_FALSE(handler.has_method());

    int received = 0;
    handler.set_method([&received](int v) { received = v; });
    EXPECT_TRUE(handler.has_method());

    handler.handle(21);
    EXPECT_EQ(received, 21);
    handler(42);
    EXPECT_EQ(received, 42);
}

TEST(TestHandler, test_handler_member_and_delegate)
{
    class Receiver
    {
        public:
            void set(int v) { value = v; }

            int value = 0;
    };
    Receiver receiver;

    Handler<int> member_handler(&receiver, &Receiver::set);
    EXPECT_TRUE(member_handler.has_method());
    member_handler.handle(7);
    EXPECT_EQ(receiver.value, 7);

    Handler<int> delegating(&member_handler);
    delegating.handle(9);
    EXPECT_EQ(receiver.value, 9);
}

} // namespace test

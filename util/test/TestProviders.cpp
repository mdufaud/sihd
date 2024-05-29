#include <gtest/gtest.h>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Providers.hpp>

struct TestStruct
{
        int size;
        std::string str;
};

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
class TestProviders: public ::testing::Test
{
    protected:
        TestProviders() { sihd::util::LoggerManager::basic(); }

        virtual ~TestProviders() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestProviders, test_provider_array)
{
    int value;
    ArrInt array = {1, 2, 3};

    ArrayProvider<int> provider(&array);
    EXPECT_TRUE(provider.provide(&value));
    EXPECT_EQ(value, array.at(0));
    EXPECT_TRUE(provider.provide(&value));
    EXPECT_EQ(value, array.at(1));
    EXPECT_TRUE(provider.provide(&value));
    EXPECT_EQ(value, array.at(2));
    EXPECT_FALSE(provider.provide(&value));
    EXPECT_EQ(value, array.at(2));
}

TEST_F(TestProviders, test_provider_fun)
{
    FunctionProvider<int> provider([](int *a) -> bool {
        *a = 10;
        return true;
    });

    int a = 0;
    EXPECT_TRUE(provider.provide(&a));
    EXPECT_EQ(a, 10);
}

TEST_F(TestProviders, test_provider_struct)
{
    TestStruct value;
    std::vector<TestStruct> lst = {{40, "hello"}, {80, "world"}};

    VectorProvider<TestStruct> provider(&lst);
    EXPECT_TRUE(provider.provide(&value));
    EXPECT_EQ(value.size, lst[0].size);
    EXPECT_EQ(value.str, lst[0].str);
    EXPECT_TRUE(provider.provide(&value));
    EXPECT_EQ(value.size, lst[1].size);
    EXPECT_EQ(value.str, lst[1].str);
}

TEST_F(TestProviders, test_provider_list)
{
    int value = 0;
    std::list<int> lst = {1, 2, 3};
    std::list<int>::iterator it = lst.begin();

    ListProvider<int> provider(&lst);
    EXPECT_TRUE(provider.provide(&value));
    EXPECT_EQ(value, *it);
    ++it;
    EXPECT_TRUE(provider.provide(&value));
    EXPECT_EQ(value, *it);
    ++it;
    EXPECT_TRUE(provider.provide(&value));
    EXPECT_EQ(value, *it);
    EXPECT_FALSE(provider.provide(&value));
    EXPECT_EQ(value, *it);
}

TEST_F(TestProviders, test_provider_vector)
{
    int value = 0;
    std::vector<int> lst = {1, 2, 3, 4};

    VectorProvider<int> provider(&lst);
    EXPECT_TRUE(provider.provide(&value));
    EXPECT_EQ(value, lst.at(0));
    EXPECT_TRUE(provider.provide(&value));
    EXPECT_EQ(value, lst.at(1));
    EXPECT_TRUE(provider.provide(&value));
    EXPECT_EQ(value, lst.at(2));
    EXPECT_TRUE(provider.provide(&value));
    EXPECT_EQ(value, lst.at(3));

    EXPECT_FALSE(provider.provide(&value));
    EXPECT_EQ(value, lst.at(3));
    EXPECT_FALSE(provider.provide(&value));
    EXPECT_EQ(value, lst.at(3));

    std::vector<int> lst2 = {10, 20};
    provider.set_container(&lst2);
    EXPECT_TRUE(provider.provide(&value));
    EXPECT_EQ(value, lst2.at(0));
    EXPECT_TRUE(provider.provide(&value));
    EXPECT_EQ(value, lst2.at(1));

    EXPECT_FALSE(provider.provide(&value));
    EXPECT_EQ(value, lst2.at(1));
    EXPECT_FALSE(provider.provide(&value));
    EXPECT_EQ(value, lst2.at(1));
}
} // namespace test

#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Named.hpp>

namespace test
{
    SIHD_LOGGER;
    using namespace sihd::util;
    class TestNamed:   public ::testing::Test
    {
        protected:
            TestNamed()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestNamed()
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

    TEST_F(TestNamed, test_named_util)
    {
        Named named("obj");
        EXPECT_EQ(named.get_name(), "obj");
        EXPECT_EQ(named.get_parent(), nullptr);
    }

    TEST_F(TestNamed, test_named_find)
    {
        Named named("obj");
        EXPECT_EQ(named.find("."), &named);
        EXPECT_EQ(named.find_node("."), nullptr);
        EXPECT_EQ(named.find(".."), nullptr);
        EXPECT_EQ(named.find("/"), nullptr);
    }
}

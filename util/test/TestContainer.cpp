#include <gtest/gtest.h>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Container.hpp>

namespace test
{
    SIHD_LOGGER;

    using namespace sihd::util;
    class TestContainer: public ::testing::Test
    {
        protected:
            TestContainer()
            {
                LoggerManager::basic();
            }

            virtual ~TestContainer()
            {
                LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }
    };

    TEST_F(TestContainer, test_container_vector)
    {
        std::vector<int> vec = {1, 2, 3, 4};

        EXPECT_TRUE(Container::contains(vec, 2));
        EXPECT_FALSE(Container::contains(vec, 5));

        EXPECT_TRUE(Container::emplace_unique(vec, 5));
        EXPECT_FALSE(Container::emplace_unique(vec, 4));
        EXPECT_EQ(vec.size(), 5u);

        EXPECT_FALSE(Container::erase(vec, 0));
        EXPECT_TRUE(Container::erase(vec, 3));
        EXPECT_EQ(vec.size(), 4u);
    }

    TEST_F(TestContainer, test_container_str)
    {
        EXPECT_EQ(Container::container_str(std::map<std::string, int>{}), "{}");
        EXPECT_EQ(Container::container_str(std::map<std::string, int>{
            {"first", 1},
        }), "{first: 1}");
        EXPECT_EQ(Container::container_str(std::map<std::string, int>{
            {"first", 1},
            {"second", 2},
        }), "{first: 1, second: 2}");
        EXPECT_EQ(Container::container_str(std::map<int, int>{
            {1, 42},
            {2, 1337},
        }), "{1: 42, 2: 1337}");

        EXPECT_EQ(Container::container_str(std::vector<int>{}), "[]");
        EXPECT_EQ(Container::container_str(std::vector<int>{1, 2, 3}), "[1, 2, 3]");
        EXPECT_EQ(Container::container_str(std::vector<std::string>{"hi"}), "[hi]");
        EXPECT_EQ(Container::container_str(std::array<std::string, 2>{"hi", "!"}), "[hi, !]");
    }
}

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
}

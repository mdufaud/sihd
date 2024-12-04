#include <gtest/gtest.h>
#include <sihd/util/Logger.hpp>
#include <sihd/util/TmpDir.hpp>
#include <sihd/util/Uuid.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/term.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::util;
class TestUuid: public ::testing::Test
{
    protected:
        TestUuid() { sihd::util::LoggerManager::stream(); }

        virtual ~TestUuid() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestUuid, test_uuid)
{
    Uuid uuid;
    ASSERT_FALSE(uuid.is_null());

    Uuid uuid2(uuid);
    EXPECT_TRUE(uuid == uuid2);
    EXPECT_EQ(uuid.str(), uuid2.str());

    Uuid uuid3(uuid.str());
    EXPECT_TRUE(uuid == uuid3);
    EXPECT_EQ(uuid.str(), uuid3.str());

    Uuid uuid4("test");
    EXPECT_TRUE(uuid4.is_null());
}
} // namespace test

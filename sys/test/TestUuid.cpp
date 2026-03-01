#include <gtest/gtest.h>
#include <sihd/util/Logger.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/Uuid.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/term.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::sys;
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
    {
        Uuid uuid;
        ASSERT_FALSE(uuid.is_null());

        Uuid uuid2(uuid);
        EXPECT_TRUE(uuid == uuid2);
        EXPECT_EQ(uuid.str(), uuid2.str());

        Uuid uuid3(uuid.str());
        EXPECT_TRUE(uuid == uuid3);
        EXPECT_EQ(uuid.str(), uuid3.str());
    }
    {
        Uuid uuid("not-a-uuid");
        EXPECT_TRUE(uuid.is_null());
    }

    {
        EXPECT_EQ(Uuid(Uuid::DNS(), "sihd").str(), "ac25224e-7775-5a3f-a8c5-efa7eb15f5fd");
        EXPECT_EQ(Uuid(Uuid::URL(), "sihd").str(), "609c7508-0214-5ac8-b045-235be114eea7");
        EXPECT_EQ(Uuid(Uuid::OID(), "sihd").str(), "5997563a-725e-5b41-b2a6-fdb389c01f16");
        EXPECT_EQ(Uuid(Uuid::X500(), "sihd").str(), "65d77e6b-3499-5b92-859e-79021df31f42");
    }

    {
        EXPECT_THROW(Uuid uuid(Uuid("not-a-uuid"), "name"), std::invalid_argument);
    }
}

} // namespace test

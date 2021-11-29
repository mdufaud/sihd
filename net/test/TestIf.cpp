#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/net/If.hpp>
#include <sihd/net/NetInterfaces.hpp>

namespace test
{
    LOGGER;
    using namespace sihd::net;
    class TestIf:   public ::testing::Test
    {
        protected:
            TestIf()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestIf()
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

    TEST_F(TestIf, test_if)
    {
        NetInterfaces ifs;
        NetIFace *lo = nullptr;

        EXPECT_FALSE(ifs.error());
        std::vector<NetIFace *> ifaces = ifs.ifaces();
        for (NetIFace *iface: ifaces)
        {
            if (iface->name() == "lo")
                lo = iface;
            LOG(debug, iface->name() << " -> " << (iface->up() ? "up" : "down"));
        }
        EXPECT_TRUE(ifaces.size() > 0);
        EXPECT_NE(lo, nullptr);
        if (lo != nullptr)
        {
            EXPECT_TRUE(lo->loopback());
        }
    }
}
#include <gtest/gtest.h>

#include <sihd/pcap/PcapInterfaces.hpp>
#include <sihd/pcap/Sniffer.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
SIHD_NEW_LOGGER("test");
using namespace sihd::pcap;
using namespace sihd::util;
class TestSniffer: public ::testing::Test
{
    protected:
        TestSniffer() { sihd::util::LoggerManager::stream(); }

        virtual ~TestSniffer() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestSniffer, test_sniffer_interfaces)
{
    PcapInterfaces pcapif;

    for (const PcapIFace & iface : pcapif.ifaces())
    {
        SIHD_LOG(debug, "Interface: {}", iface.dump());
        for (const auto & addr : iface.addresses())
        {
            if (!addr.addr.empty())
                SIHD_LOG(debug, "Addr: {}", addr.addr.str());
            if (!addr.broadaddr.empty())
                SIHD_LOG(debug, "    broadcast: {}", addr.broadaddr.str());
            if (!addr.dstaddr.empty())
                SIHD_LOG(debug, "    dst: {}", addr.dstaddr.str());
            if (!addr.netmask.empty())
                SIHD_LOG(debug, "    netmask: {}", addr.netmask.str());
        }
    }
}

} // namespace test

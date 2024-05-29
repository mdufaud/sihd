#include <gtest/gtest.h>
#include <sihd/net/IpAddr.hpp>
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
        TestSniffer() { sihd::util::LoggerManager::basic(); }

        virtual ~TestSniffer() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}

        void dump_ip(const std::string & pre, sockaddr *addr)
        {
            sihd::net::IpAddr ip(*addr);

            if (!ip.empty())
                SIHD_LOG(debug, "{} {}", pre, ip.first_ip_str());
        }
};

TEST_F(TestSniffer, test_sniffer_interfaces)
{
    PcapInterfaces pcapif;

    for (const PcapIFace & iface : pcapif.ifaces())
    {
        SIHD_LOG(debug, "Interface: {}", iface.dump());
        for (const auto & addr : iface.addresses())
        {
            dump_ip("Addr:", addr->addr);
            if (addr->broadaddr)
                dump_ip("    broadcast:", addr->broadaddr);
            if (addr->dstaddr)
                dump_ip("    dst:", addr->dstaddr);
            if (addr->netmask)
                dump_ip("    netmask:", addr->netmask);
        }
    }
}

} // namespace test

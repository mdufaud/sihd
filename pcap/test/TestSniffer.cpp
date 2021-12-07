#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/pcap/PcapInterfaces.hpp>
#include <sihd/pcap/Sniffer.hpp>
#include <sihd/net/IpAddr.hpp>
#include <sihd/util/Handler.hpp>

#include <sihd/util/.hpp>

namespace test
{
    NEW_LOGGER("test");
    using namespace sihd::pcap;
    using namespace sihd::util;
    class TestSniffer:   public ::testing::Test
    {
        protected:
            TestSniffer()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestSniffer()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }

            void dump_ip(const std::string & pre, sockaddr *addr)
            {
                sihd::net::IpAddr ip(*addr);

                if (!ip.empty())
                    LOG(debug, pre << ip.get_first_ip());
            }
    };

    TEST_F(TestSniffer, test_sniffer_interfaces)
    {
        PcapInterfaces pcapif;

        for (const PcapIFace & iface: pcapif.ifaces())
        {
            LOG(debug, "Interface: " << iface.dump());
            for (const auto & addr: iface.addresses())
            {
                dump_ip("Addr: ", addr->addr);
                if (addr->broadaddr)
                    dump_ip("    broadcast: ", addr->broadaddr);
                if (addr->dstaddr)
                    dump_ip("    dst: ", addr->dstaddr);
                if (addr->netmask)
                    dump_ip("    netmask: ", addr->netmask);
            }
        }
    }

}
#include "sihd/net/IpAddr.hpp"
#include <netdb.h> // addrinfo

#include <atomic>

#include <gtest/gtest.h>

#include <sihd/net/IcmpSender.hpp>
#include <sihd/net/dns.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Worker.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/platform.hpp>
#include <sihd/util/term.hpp>

namespace test
{

SIHD_LOGGER;
using namespace sihd::net;
using namespace sihd::util;
using namespace sihd::sys;
class TestIcmp: public ::testing::Test
{
    protected:
        TestIcmp() { sihd::util::LoggerManager::stream(); }

        virtual ~TestIcmp() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() { _seq = 1; }

        virtual void TearDown() {}

        void test_icmp(IcmpSender & sender)
        {
            sender.set_poll_timeout(500); // Augmente le timeout pour IPv6
            sender.set_id(getpid());
            sender.set_ttl(64);
            sender.set_data_size(56u);
            time_t now = sihd::util::Clock::default_clock.now();
            sender.set_data({&now, sizeof(now)});
            sender.set_seq(1);

            bool reached = false;
            std::atomic<bool> response_received {false};

            sihd::util::Handler<IcmpSender *> handler([&](IcmpSender *sender) {
                const IcmpResponse & response = sender->response();
                EXPECT_EQ(response.size, 56u);
                EXPECT_EQ(response.seq, 1); // Sequence is what identifies the ping
                // For IPv6 SOCK_DGRAM, kernel assigns its own ID (socket identifier)
                // For IPv4 SOCK_RAW, we control the ID and it should match getpid() (truncated to 16 bits)
                if (!sender->socket().is_ipv6())
                {
                    EXPECT_EQ(response.id, getpid() & 0xFFFF);
                }

                time_t timestamp = ((time_t *)response.data)[0];
                int64_t elapsed_ms
                    = sihd::util::time::to_ms(sihd::util::Clock::default_clock.now() - timestamp);

                SIHD_LOG(info,
                         "ICMP response: type={} code={} ttl={} id={} seq={} time={}ms from {}",
                         response.type,
                         response.code,
                         response.ttl,
                         response.id,
                         response.seq,
                         elapsed_ms,
                         response.client.str());

                // Sequence identifies the ping; ID only matters for IPv4
                reached = (response.seq == 1)
                          && (sender->socket().is_ipv6() || response.id == (getpid() & 0xFFFF));
                response_received = true;
            });
            sender.add_observer(&handler);

            sihd::util::Worker worker([&sender] { return sender.start(); });
            ASSERT_TRUE(worker.start_sync_worker("receiver"));

            // Give receiver time to start
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            EXPECT_TRUE(this->send(sender, _host_ipaddr));

            // Wait up to 5 seconds for response
            int wait_count = 0;
            while (!response_received && wait_count < 50)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                ++wait_count;
            }

            sender.stop();
            EXPECT_TRUE(worker.stop_worker());

            EXPECT_TRUE(reached) << "No ICMP response received from " << _host_ipaddr.str();
        }

        bool send(IcmpSender & sender, const IpAddr & host)
        {
            time_t now = sihd::util::Clock::default_clock.now();
            sender.set_data({&now, sizeof(now)});

            sender.set_seq(_seq);
            if (!sender.send_to(host))
            {
                SIHD_LOG(error, "Failed to send ICMP packet to {}", host.str());
                return false;
            }

            SIHD_LOG(info, "Sent ICMP echo request to {} (seq={})", host.str(), _seq);
            ++_seq;
            return true;
        }

        size_t _seq;
        IpAddr _host_ipaddr;
};

TEST_F(TestIcmp, test_icmp_ipv4)
{
    _host_ipaddr = dns::find("google.com", false);
    IcmpSender sender("icmp-sender");
    sender.set_echo();
    if (sender.open_socket(false) == false)
    {
        GTEST_SKIP() << "Must be root or have capabilities to do the test\n"
                     << "execute command: 'sudo setcap cap_net_raw=pe " << fs::executable_path() << "'";
    }
    this->test_icmp(sender);
}

TEST_F(TestIcmp, test_icmp_ipv6)
{
    _host_ipaddr = dns::find("google.com", true);
    IcmpSender sender("icmp-sender");
    sender.set_echo(); // to open SOCK_DGRAM on IPv6 thus avoiding raw socket requirement
    ASSERT_TRUE(sender.open_socket(true));
    this->test_icmp(sender);
}
} // namespace test

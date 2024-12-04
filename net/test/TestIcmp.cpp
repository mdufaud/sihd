#include <netdb.h> // addrinfo

#include <gtest/gtest.h>

#include <sihd/net/IcmpSender.hpp>
#include <sihd/net/dns.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Worker.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/term.hpp>

namespace test
{

SIHD_LOGGER;
using namespace sihd::net;
using namespace sihd::util;
class TestIcmp: public ::testing::Test
{
    protected:
        TestIcmp() { sihd::util::LoggerManager::stream(); }

        virtual ~TestIcmp() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() { _seq = 1; }

        virtual void TearDown() {}

        void test_icmp(IcmpSender & sender)
        {
            sender.set_echo();
            sender.set_poll_timeout(1);
            sender.set_id(getpid());
            sender.set_ttl(64);
            // default size for echo packets
            sender.set_data_size(56u);

            bool reached = false;
            sihd::util::Handler<IcmpSender *> handler([&](IcmpSender *sender) {
                const IcmpResponse & response = sender->response();
                ASSERT_EQ(response.size, 56u);
                time_t timestamp = ((time_t *)response.data)[0];
                reached = response.id == getpid();
                SIHD_LOG_DEBUG("code={} type={} ttl={} id={} ({}) seq={} time={}ms",
                               response.type,
                               response.code,
                               response.ttl,
                               response.id,
                               getpid(),
                               response.seq,
                               sihd::util::time::to_ms(sihd::util::Clock::default_clock.now() - timestamp));
                // make a DNS lookup on client
                auto dns = dns::lookup(response.client.str());
                SIHD_LOG_DEBUG("{}", dns.hostname);
                SIHD_LOG_DEBUG("{}", dns.host);
            });
            sender.add_observer(&handler);

            sihd::util::Worker worker([&sender] { return sender.start(); });
            SIHD_LOG(debug, "Starting receiver");
            ASSERT_TRUE(worker.start_sync_worker("receiver"));

            this->send(sender, _client_host);

            std::this_thread::sleep_for(std::chrono::seconds(2));

            sender.stop();
            EXPECT_TRUE(worker.stop_worker());

            EXPECT_TRUE(reached);
        }

        void send(IcmpSender & sender, std::string_view host)
        {
            time_t now = sihd::util::Clock::default_clock.now();
            EXPECT_TRUE(sender.set_data({&now, sizeof(now)}));

            sender.set_seq(_seq);
            EXPECT_TRUE(sender.send_to({host, true}));
            usleep(10E3);
            ++_seq;
        }

        size_t _seq;
        std::string _client_host;
};

TEST_F(TestIcmp, test_icmp_ipv6)
{
    _client_host = "google.com";
    IcmpSender sender("icmp-sender");
    if (sender.open_socket(true) == false)
    {
        GTEST_SKIP() << "Must be root or have capabilities to do the test\n"
                     << "execute command: 'sudo setcap cap_net_raw=pe " << fs::executable_path() << "'\n";
    }
    this->test_icmp(sender);
}

TEST_F(TestIcmp, test_icmp_ipv4)
{
    _client_host = "google.com";
    IcmpSender sender("icmp-sender");
    if (sender.open_socket(false) == false)
    {
        GTEST_SKIP() << "Must be root or have capabilities to do the test\n"
                     << "execute command: 'sudo setcap cap_net_raw=pe " << fs::executable_path() << "'\n";
    }
    this->test_icmp(sender);
}
} // namespace test

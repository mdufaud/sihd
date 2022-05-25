#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/FS.hpp>
#include <sihd/util/OS.hpp>
#include <sihd/util/Term.hpp>
#include <sihd/net/IcmpSender.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Worker.hpp>
#include <sihd/util/ObserverWaiter.hpp>

namespace test
{
    SIHD_LOGGER;
    using namespace sihd::net;
    using namespace sihd::util;
    class TestIcmp: public ::testing::Test
    {
        protected:
            TestIcmp()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestIcmp()
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

    TEST_F(TestIcmp, test_icmp_ipv4)
    {
        IcmpSender sender("icmp-sender");

        if (sender.open_socket(false) == false)
            GTEST_SKIP() << "Must be root or have capabilities to do the test";

        sender.set_echo();

        sihd::util::ObserverWaiter obs(&sender);

        bool reached = false;
        sihd::util::Handler<IcmpSender *> handler([&reached] (IcmpSender *sender)
        {
            const IcmpResponse & response = sender->response();
            reached = response.reached;
            SIHD_LOG_DEBUG("code=%d type=%d ttl=%d id=%d seq=%d time=%ld",
                            response.type,
                            response.code,
                            response.ttl,
                            response.id,
                            response.seq,
                            sihd::util::Time::to_ms(sender->clock()->now() - response.timestamp));
            // make a DNS lookup on client
            IpAddr client(response.client.host(), true);
            SIHD_LOG_DEBUG(client.hostname());
            SIHD_LOG_DEBUG(client.host());
        });
        sender.add_observer(&handler);
        sender.set_poll_timeout(1);

        sihd::util::Worker worker(&sender);
        SIHD_LOG(debug, "Starting receiver");
        EXPECT_TRUE(worker.start_sync_worker("receiver"));

        EXPECT_TRUE(sender.send_to({"google.com", true}));

        usleep(10E3);

        sender.stop();
        EXPECT_TRUE(worker.stop_worker());

        EXPECT_TRUE(reached);
    }
}
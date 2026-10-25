#include <string>

#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>
#include <sihd/wifi/connection.hpp>
#include <sihd/wifi/scan.hpp>

#include "fake_daemon.hpp"

namespace test
{
using namespace sihd::util;
namespace wifi = sihd::wifi;

class TestIwd: public ::testing::Test
{
    protected:
        TestIwd() { LoggerManager::stream(); }

        virtual ~TestIwd() { LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            if (!FakeDaemon::available())
                GTEST_SKIP() << "no dbus-daemon";
            ASSERT_TRUE(_daemon.start({iwd_service}));
            serve_iwd_station(_daemon, "disconnected");

            this->add_network(home, "home", "psk", known_home);
            this->add_network(attic, "attic", "psk", "");
            this->add_network(corp, "corp", "8021x", "");
            this->add_network(cafe, "cafe", "open", "");
            _daemon.set_property(known_home, known_interface, "Name", text("home"));
            _daemon.set_property(known_home, known_interface, "Type", text("psk"));

            _daemon.on(agent_manager_interface, "RegisterAgent", [this](const dbus::Message & call) {
                _daemon.reply(call, {});
            });
            _daemon.on(agent_manager_interface, "UnregisterAgent", [this](const dbus::Message & call) {
                _daemon.reply(call, {});
            });
            _daemon.on(known_interface, "Forget", [this](const dbus::Message & call) { _daemon.reply(call, {}); });
        }

        void add_network(const std::string & path,
                         const std::string & name,
                         const std::string & type,
                         const std::string & known)
        {
            _daemon.set_property(path, network_interface, "Name", text(name));
            _daemon.set_property(path, network_interface, "Type", text(type));
            _daemon.set_property(path, network_interface, "Device", object_path(iwd_station));
            if (!known.empty())
                _daemon.set_property(path, network_interface, "KnownNetwork", object_path(known));
        }

        Result<std::string> request_passphrase(dbus::Connection & sender, const dbus::Message & connect_call)
        {
            auto request = dbus::Message::method_call(connect_call.sender(),
                                                      "/agent",
                                                      agent_interface,
                                                      "RequestPassphrase");
            SIHD_UNEXPECTED_RETURN(request);
            auto added = request->add_object_path(connect_call.path());
            SIHD_UNEXPECTED_RETURN(added);
            auto answer = request->call(sender, fake_timeout);
            SIHD_UNEXPECTED_RETURN(answer);
            return dbus::Iter(*answer).get<std::string>();
        }

        static constexpr char home[] = "/net/connman/iwd/0/4/686f6d65_psk";
        static constexpr char attic[] = "/net/connman/iwd/0/4/6174746963_psk";
        static constexpr char corp[] = "/net/connman/iwd/0/4/636f7270_8021x";
        static constexpr char cafe[] = "/net/connman/iwd/0/4/63616665_open";
        static constexpr char known_home[] = "/net/connman/iwd/686f6d65_psk";
        static constexpr char network_interface[] = "net.connman.iwd.Network";
        static constexpr char known_interface[] = "net.connman.iwd.KnownNetwork";
        static constexpr char agent_manager_interface[] = "net.connman.iwd.AgentManager";
        static constexpr char agent_interface[] = "net.connman.iwd.Agent";

        FakeDaemon _daemon;
};

TEST_F(TestIwd, test_backend_detection)
{
    _daemon.serve();

    auto managing = wifi::backend("wlan-test");
    auto unknown = wifi::backend("eth-test");

    ASSERT_TRUE(managing.has_value()) << managing.error().message;
    EXPECT_EQ(*managing, wifi::Backend::iwd);
    ASSERT_FALSE(unknown.has_value());
    EXPECT_EQ(unknown.error().code, ErrorCode::not_found);
    EXPECT_EQ(unknown.error().message, "no wifi daemon manages eth-test");
}

TEST_F(TestIwd, test_state)
{
    _daemon.set_property(iwd_station, iwd_station_interface, "State", text("roaming"));
    _daemon.serve();

    auto current = wifi::state("wlan-test");

    ASSERT_TRUE(current.has_value()) << current.error().message;
    EXPECT_EQ(*current, wifi::State::connected);
}

TEST_F(TestIwd, test_scan_waits_for_scanning_end)
{
    int scans = 0;
    _daemon.on(iwd_station_interface, "Scan", [this, &scans](const dbus::Message & call) {
        if (scans++ == 0)
        {
            _daemon.reply(call, {});
            _daemon.emit_properties_changed(iwd_station, iwd_station_interface, {{"Scanning", boolean(true)}});
        }
        else
            _daemon.fail(call, "net.connman.iwd.Busy", "Operation already in progress");
        _daemon.emit_properties_changed(iwd_station, iwd_station_interface, {{"Scanning", boolean(false)}});
    });
    _daemon.serve();

    auto scanned = wifi::scan("wlan-test", fake_timeout);
    auto while_busy = wifi::scan("wlan-test", fake_timeout);

    ASSERT_TRUE(scanned.has_value()) << scanned.error().message;
    ASSERT_TRUE(while_busy.has_value()) << while_busy.error().message;
}

TEST_F(TestIwd, test_connect_known_network)
{
    _daemon.on(network_interface, "Connect", [this](const dbus::Message & call) { _daemon.reply(call, {}); });
    _daemon.serve();

    auto connected = wifi::connect("wlan-test", "home", fake_timeout);

    _daemon.stop();
    ASSERT_TRUE(connected.has_value()) << connected.error().message;
    EXPECT_FALSE(_daemon.called(agent_manager_interface, "RegisterAgent"));
    EXPECT_FALSE(_daemon.called(known_interface, "Forget"));
}

TEST_F(TestIwd, test_connect_serves_the_passphrase)
{
    std::string received_passphrase;
    _daemon.on(network_interface, "Connect", [&](const dbus::Message & call) {
        auto passphrase = this->request_passphrase(_daemon.connection(), call);
        received_passphrase = passphrase.value_or("");
        _daemon.reply(call, {});
    });
    _daemon.serve();

    auto connected = wifi::connect("wlan-test", "attic", "an attic passphrase", fake_timeout);

    _daemon.stop();
    ASSERT_TRUE(connected.has_value()) << connected.error().message;
    EXPECT_EQ(received_passphrase, "an attic passphrase");
    EXPECT_TRUE(_daemon.called(agent_manager_interface, "RegisterAgent"));
    EXPECT_TRUE(_daemon.called(agent_manager_interface, "UnregisterAgent"));
    EXPECT_FALSE(_daemon.called(known_interface, "Forget"));
}

TEST_F(TestIwd, test_agent_refuses_strangers)
{
    std::string stranger_error;
    _daemon.on(network_interface, "Connect", [&](const dbus::Message & call) {
        dbus::Connection stranger;
        ASSERT_TRUE(stranger.open(shared_bus()->address()).has_value());
        auto stolen = this->request_passphrase(stranger, call);
        stranger_error = stolen.has_value() ? *stolen : stolen.error().message;
        _daemon.reply(call, {});
    });
    _daemon.serve();

    auto connected = wifi::connect("wlan-test", "attic", "a secret passphrase", fake_timeout);

    _daemon.stop();
    ASSERT_TRUE(connected.has_value()) << connected.error().message;
    EXPECT_EQ(stranger_error, "net.connman.iwd.Agent.Error.Canceled: this agent only answers iwd");
}

TEST_F(TestIwd, test_connect_refusals)
{
    _daemon.serve();

    auto unseen = wifi::connect("wlan-test", "elsewhere", fake_timeout);
    auto known = wifi::connect("wlan-test", "home", "a new passphrase", fake_timeout);
    auto secured = wifi::connect("wlan-test", "attic", fake_timeout);
    auto enterprise = wifi::connect("wlan-test", "corp", "a corp passphrase", fake_timeout);
    auto open = wifi::connect("wlan-test", "cafe", "a cafe passphrase", fake_timeout);

    _daemon.stop();
    ASSERT_FALSE(unseen.has_value());
    EXPECT_EQ(unseen.error().code, ErrorCode::not_found);
    EXPECT_EQ(unseen.error().message, "iwd did not see elsewhere");

    ASSERT_FALSE(known.has_value());
    EXPECT_EQ(known.error().code, ErrorCode::already_exists);
    EXPECT_EQ(known.error().message, "home is already known: forget it first");

    ASSERT_FALSE(secured.has_value());
    EXPECT_EQ(secured.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(secured.error().message, "attic needs a passphrase");

    ASSERT_FALSE(enterprise.has_value());
    EXPECT_EQ(enterprise.error().code, ErrorCode::not_supported);
    EXPECT_EQ(enterprise.error().message, "corp needs 802.1x, which is not supported");

    ASSERT_FALSE(open.has_value());
    EXPECT_EQ(open.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(open.error().message, "cafe takes no passphrase");
    EXPECT_FALSE(_daemon.called(network_interface, "Connect"));
    EXPECT_FALSE(_daemon.called(known_interface, "Forget"));
}

TEST_F(TestIwd, test_connect_errors)
{
    int connects = 0;
    _daemon.on(network_interface, "Connect", [this, &connects](const dbus::Message & call) {
        if (connects == 0)
            _daemon.fail(call, "net.connman.iwd.Failed", "Operation failed");
        else if (connects == 1)
            _daemon.fail(call, "org.freedesktop.DBus.Error.AccessDenied", "not allowed");
        ++connects;
    });
    _daemon.serve();

    auto refused = wifi::connect("wlan-test", "attic", "a wrong passphrase", fake_timeout);
    auto denied = wifi::connect("wlan-test", "home", fake_timeout);
    auto timed_out = wifi::connect("wlan-test", "cafe", Duration(time::ms(200)));

    _daemon.stop();
    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error().code, ErrorCode::io_error);
    EXPECT_EQ(refused.error().message, "net.connman.iwd.Failed: Operation failed: connecting to attic");
    ASSERT_FALSE(denied.has_value());
    EXPECT_EQ(denied.error().code, ErrorCode::permission_denied);
    EXPECT_EQ(denied.error().message, "org.freedesktop.DBus.Error.AccessDenied: not allowed: connecting to home");
    ASSERT_FALSE(timed_out.has_value());
    EXPECT_EQ(timed_out.error().code, ErrorCode::timeout);
    EXPECT_EQ(timed_out.error().message, "dbus receive timed out: connecting to cafe");
    EXPECT_TRUE(_daemon.called(agent_manager_interface, "UnregisterAgent"));
}

TEST_F(TestIwd, test_known_networks_and_forget)
{
    _daemon.serve();

    auto known = wifi::known_networks("wlan-test");
    auto forgotten = wifi::forget("wlan-test", "home");
    auto unknown = wifi::forget("wlan-test", "attic");

    _daemon.stop();
    ASSERT_TRUE(known.has_value()) << known.error().message;
    ASSERT_EQ(known->size(), 1u);
    EXPECT_EQ(known->front().ssid, "home");
    EXPECT_EQ(known->front().security, wifi::Security::personal);
    ASSERT_TRUE(forgotten.has_value()) << forgotten.error().message;
    EXPECT_TRUE(_daemon.called(known_interface, "Forget"));
    ASSERT_FALSE(unknown.has_value());
    EXPECT_EQ(unknown.error().code, ErrorCode::not_found);
    EXPECT_EQ(unknown.error().message, "no known network attic");
}

TEST_F(TestIwd, test_disconnect_not_connected)
{
    _daemon.on(iwd_station_interface, "Disconnect", [this](const dbus::Message & call) {
        _daemon.fail(call, "net.connman.iwd.NotConnected", "Not connected");
    });
    _daemon.serve();

    auto disconnected = wifi::disconnect("wlan-test");

    ASSERT_TRUE(disconnected.has_value()) << disconnected.error().message;
}

} // namespace test

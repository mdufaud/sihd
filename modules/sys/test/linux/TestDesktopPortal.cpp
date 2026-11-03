#include <array>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/desktop.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/linux/dbus.hpp>
#include <sihd/util/Logger.hpp>

#include "../test_helper.hpp"
#include "private_dbus.hpp"

namespace test
{
using namespace sihd::util;
using namespace sihd::sys;
using namespace sihd::sys::linux_::dbus;

namespace
{

std::string read_all_fd(const UnixFd & fd)
{
    std::array<char, 64> buffer {};
    const ssize_t size = read_fd(fd.fd(), buffer.data(), buffer.size());
    return size > 0 ? std::string(buffer.data(), static_cast<size_t>(size)) : std::string();
}

} // namespace

class TestDesktopPortal: public ::testing::Test
{
    protected:
        TestDesktopPortal() { LoggerManager::stream(); }

        virtual ~TestDesktopPortal() { LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            if constexpr (!desktop::supported)
            {
                GTEST_SKIP() << "desktop services not supported on this platform";
            }
            if (!PrivateDBus::available())
            {
                GTEST_SKIP() << "no dbus-daemon";
            }
            ASSERT_TRUE(_bus.start());
            auto opened = _server.open(_bus.address());
            ASSERT_TRUE(opened.has_value()) << opened.error().message;
            auto owned = _server.request_name("org.freedesktop.portal.Desktop");
            ASSERT_TRUE(owned.has_value()) << owned.error().message;
        }

        void answer_handle_then_response(const Message & call, uint32_t response)
        {
            auto reply = call.method_return();
            ASSERT_TRUE(reply.has_value());
            ASSERT_TRUE(reply->add_object_path(portal_handle).has_value());
            ASSERT_TRUE(reply->send(_server).has_value());

            auto signal = Message::signal(portal_handle, "org.freedesktop.portal.Request", "Response");
            ASSERT_TRUE(signal.has_value());
            Appender body(*signal);
            ASSERT_TRUE(body.add(response).has_value());
            auto results = body.open_array("{sv}");
            ASSERT_TRUE(results.has_value());
            ASSERT_TRUE(body.close(std::move(*results)).has_value());
            ASSERT_TRUE(signal->send(_server).has_value());
        }

        void check_read_one_scheme(uint32_t scheme, const std::string & expected)
        {
            PrivateDBusClient client;
            ASSERT_TRUE(client.start(_bus.address(), {"desktop", "action=color_scheme"}));

            auto call = receive_member(_server, "ReadOne");
            ASSERT_TRUE(call.has_value());
            EXPECT_EQ(call->interface(), "org.freedesktop.portal.Settings");
            EXPECT_EQ(read_strings(*call), (std::vector<std::string> {"org.freedesktop.appearance", "color-scheme"}));
            answer_u32_variant(_server, *call, scheme);

            EXPECT_EQ(client.next_line(), expected);
            EXPECT_EQ(client.finish(), 0);
        }

        void check_opened_by_fd(const std::string & action, const std::string & member)
        {
            TmpDir tmp_dir;
            const std::string path = fs::combine(tmp_dir.path(), "file.txt");
            ASSERT_TRUE(fs::write(path, "file content"));
            PrivateDBusClient client;
            ASSERT_TRUE(client.start(_bus.address(), {"desktop", "action=" + action, "target=" + path}));

            auto call = receive_member(_server, member);
            ASSERT_TRUE(call.has_value());
            EXPECT_EQ(call->interface(), "org.freedesktop.portal.OpenURI");
            EXPECT_EQ(call->signature(), "sha{sv}");
            Iter args(*call);
            args.next();
            auto fd = args.get_unix_fd();
            ASSERT_TRUE(fd.has_value()) << fd.error().message;
            EXPECT_EQ(read_all_fd(*fd), "file content");
            this->answer_handle_then_response(*call, 0);

            EXPECT_EQ(client.next_line(), "ok");
            EXPECT_EQ(client.finish(), 0);
        }

        void check_response(uint32_t response, const std::string & expected)
        {
            PrivateDBusClient client;
            ASSERT_TRUE(client.start(_bus.address(), {"desktop", "action=open_uri", "target=https://example.org/"}));

            auto call = receive_member(_server, "OpenURI");
            ASSERT_TRUE(call.has_value());
            this->answer_handle_then_response(*call, response);

            EXPECT_EQ(client.next_line(), expected);
        }

        PrivateDBus _bus;
        Connection _server;
};

TEST_F(TestDesktopPortal, test_open_uri)
{
    PrivateDBusClient client;
    ASSERT_TRUE(client.start(_bus.address(), {"desktop", "action=open_uri", "target=https://example.org/?a=b"}));

    auto call = receive_member(_server, "OpenURI");
    ASSERT_TRUE(call.has_value());
    EXPECT_EQ(call->path(), "/org/freedesktop/portal/desktop");
    EXPECT_EQ(call->interface(), "org.freedesktop.portal.OpenURI");
    EXPECT_EQ(call->signature(), "ssa{sv}");
    Iter args(*call);
    EXPECT_EQ(args.get<std::string>().value_or("unreadable"), "");
    args.next();
    EXPECT_EQ(args.get<std::string>().value_or("unreadable"), "https://example.org/?a=b");
    this->answer_handle_then_response(*call, 0);

    EXPECT_EQ(client.next_line(), "ok");
    EXPECT_EQ(client.finish(), 0);
}

TEST_F(TestDesktopPortal, test_open_file_sends_an_fd)
{
    this->check_opened_by_fd("open_file", "OpenFile");
}

TEST_F(TestDesktopPortal, test_reveal_sends_an_fd)
{
    this->check_opened_by_fd("reveal", "OpenDirectory");
}

TEST_F(TestDesktopPortal, test_cancelled_response)
{
    this->check_response(1, error_line(ErrorCode::permission_denied, "the desktop portal request was cancelled"));
}

TEST_F(TestDesktopPortal, test_failed_response)
{
    this->check_response(2, error_line(ErrorCode::io_error, "the desktop portal request failed"));
}

TEST_F(TestDesktopPortal, test_color_scheme)
{
    this->check_read_one_scheme(0, "scheme=no_preference");
    this->check_read_one_scheme(1, "scheme=dark");
    this->check_read_one_scheme(2, "scheme=light");
    this->check_read_one_scheme(7, "scheme=no_preference");
}

TEST_F(TestDesktopPortal, test_color_scheme_of_an_old_portal)
{
    PrivateDBusClient client;
    ASSERT_TRUE(client.start(_bus.address(), {"desktop", "action=color_scheme"}));

    auto read_one = receive_member(_server, "ReadOne");
    ASSERT_TRUE(read_one.has_value());
    refuse(_server, *read_one, "org.freedesktop.DBus.Error.UnknownMethod");

    auto read = receive_member(_server, "Read");
    ASSERT_TRUE(read.has_value());
    auto reply = read->method_return();
    ASSERT_TRUE(reply.has_value());
    Appender body(*reply);
    auto outer = body.open_variant("v");
    ASSERT_TRUE(outer.has_value());
    auto inner = outer->open_variant("u");
    ASSERT_TRUE(inner.has_value());
    ASSERT_TRUE(inner->add(uint32_t {1}).has_value());
    ASSERT_TRUE(outer->close(std::move(*inner)).has_value());
    ASSERT_TRUE(body.close(std::move(*outer)).has_value());
    ASSERT_TRUE(reply->send(_server).has_value());

    EXPECT_EQ(client.next_line(), "scheme=dark");
    EXPECT_EQ(client.finish(), 0);
}

TEST_F(TestDesktopPortal, test_color_scheme_unknown_to_the_backend)
{
    PrivateDBusClient client;
    ASSERT_TRUE(client.start(_bus.address(), {"desktop", "action=color_scheme"}));

    auto call = receive_member(_server, "ReadOne");
    ASSERT_TRUE(call.has_value());
    refuse(_server, *call, "org.freedesktop.portal.Error.NotFound");

    EXPECT_EQ(client.next_line(), "scheme=no_preference");
    EXPECT_EQ(client.finish(), 0);
}

} // namespace test

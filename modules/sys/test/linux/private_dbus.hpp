#ifndef __SIHD_SYS_TEST_PRIVATE_DBUS_HPP__
#define __SIHD_SYS_TEST_PRIVATE_DBUS_HPP__

#include <array>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <sihd/sys/Poll.hpp>
#include <sihd/sys/Process.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/linux/dbus.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/time.hpp>

#include "../test_helper.hpp"

namespace test
{

// no servicedir: nothing on this bus can activate a real service of the desktop
inline constexpr const char *private_dbus_config = R"(<!DOCTYPE busconfig PUBLIC
 "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <type>session</type>
  <listen>unix:path={}/bus</listen>
  <policy context="default">
    <allow send_destination="*" eavesdrop="true"/>
    <allow eavesdrop="true"/>
    <allow own="*"/>
    <deny own="org.sihd.Denied"/>
  </policy>
</busconfig>
)";

class PrivateDBus
{
    public:
        ~PrivateDBus() { _daemon.terminate(); }

        static bool available() { return sihd::sys::os::exists_in_path("dbus-daemon"); }

        bool start()
        {
            const std::string config_path = sihd::sys::fs::combine(_tmp_dir.path(), "bus.conf");
            if (!sihd::sys::fs::write(config_path, fmt::format(private_dbus_config, _tmp_dir.path())))
                return false;
            _daemon.add_argv(std::vector<std::string> {"dbus-daemon",
                                                       "--config-file=" + config_path,
                                                       "--nofork",
                                                       "--print-address=1"});
            _daemon.stdout_to(_address);
            if (!_daemon.execute() || !_daemon.read_pipes(5000) || !_address.ends_with('\n'))
                return false;
            _address.pop_back();
            return true;
        }

        const std::string & address() const { return _address; }

    private:
        sihd::sys::TmpDir _tmp_dir;
        sihd::sys::Process _daemon;
        std::string _address;
};

class PrivateDBusClient
{
    public:
        ~PrivateDBusClient()
        {
            if (_stdin_write >= 0)
                close_fd(_stdin_write);
            _process.terminate();
        }

        bool start(const std::string & address, const std::vector<std::string> & args)
        {
            std::array<int, 2> fds {-1, -1};
            if (!make_fd_pair(fds))
                return false;
            _stdin_write = fds[1];
            _process.add_argv(helper_path());
            _process.add_argv(args);
            _process.env_set("DBUS_SESSION_BUS_ADDRESS", address);
            _process.env_set("DBUS_SYSTEM_BUS_ADDRESS", address);
            _process.stdin_from(fds[0]);
            _process.stdout_to(_output);
            const bool started = _process.execute().has_value();
            close_fd(fds[0]);
            return started;
        }

        // nullopt once nothing came for 5 seconds
        std::optional<std::string> next_line()
        {
            while (_output.find('\n', _consumed) == std::string::npos)
            {
                if (!_process.read_pipes(5000))
                    return std::nullopt;
            }
            const size_t end = _output.find('\n', _consumed);
            std::string line = _output.substr(_consumed, end - _consumed);
            _consumed = end + 1;
            return line;
        }

        bool proceed() { return write_fd(_stdin_write, "\n", 1) == 1; }

        int finish()
        {
            // the helper inherited the write end: closing it here would not reach its stdin
            EXPECT_TRUE(this->proceed());
            EXPECT_TRUE(_process.wait_any().value_or(false));
            EXPECT_TRUE(_process.terminate());
            return _process.return_code();
        }

    private:
        sihd::sys::Process _process;
        std::string _output;
        size_t _consumed = 0;
        int _stdin_write = -1;
};

// the read end sees EOF once every copy of the write end closed, wherever the bus sent them
inline bool closed_within(int read_end, int milliseconds)
{
    sihd::sys::Poll poll;
    char byte = 0;
    return poll.set_limit(1) && poll.set_read_fd(read_end) && poll.poll(milliseconds) > 0
           && read_fd(read_end, &byte, 1) == 0;
}

// skips what the bus sends unasked (NameAcquired...): nullopt on timeout
inline std::optional<sihd::sys::linux_::dbus::Message>
    receive_accepted(sihd::sys::linux_::dbus::Connection & connection,
                     const std::function<bool(const sihd::sys::linux_::dbus::Message &)> & accepted)
{
    sihd::sys::linux_::dbus::Message message;
    auto received = message.receive_until(
        connection,
        sihd::util::Duration(sihd::util::time::sec(5)),
        [&accepted](const sihd::sys::linux_::dbus::Message & candidate) -> sihd::util::Result<bool> {
            return accepted(candidate);
        });
    if (!received.has_value())
        return std::nullopt;
    return message;
}

template <typename T>
std::string error_of(const sihd::util::Result<T> & result)
{
    return result.has_value() ? "no error" : result.error().message;
}

template <typename T>
sihd::util::ErrorCode code_of(const sihd::util::Result<T> & result)
{
    return result.has_value() ? sihd::util::ErrorCode::none : result.error().code;
}

inline std::optional<sihd::sys::linux_::dbus::Message> receive_member(sihd::sys::linux_::dbus::Connection & connection,
                                                                      const std::string & member)
{
    return receive_accepted(connection, [&member](const sihd::sys::linux_::dbus::Message & message) {
        return message.member() == member;
    });
}

inline std::vector<std::string> read_strings(const sihd::sys::linux_::dbus::Message & message)
{
    std::vector<std::string> strings;
    for (sihd::sys::linux_::dbus::Iter args(message); args.type() != '\0'; args.next())
        strings.push_back(args.get<std::string>().value_or("unreadable"));
    return strings;
}

inline std::vector<std::string> answer_inhibit_with_pipe(sihd::sys::linux_::dbus::Connection & server, int & read_end)
{
    auto call = receive_member(server, "Inhibit");
    if (!call.has_value())
        return {};
    std::array<int, 2> fds {-1, -1};
    if (!make_fd_pair(fds))
        return {};
    read_end = fds[0];
    auto reply = call->method_return();
    const bool sent = reply.has_value() && reply->add_unix_fd(fds[1]).has_value() && reply->send(server).has_value();
    close_fd(fds[1]);
    return sent ? read_strings(*call) : std::vector<std::string> {};
}

inline std::optional<sihd::sys::linux_::dbus::Message> receive_reply(sihd::sys::linux_::dbus::Connection & connection)
{
    return receive_accepted(connection, [](const sihd::sys::linux_::dbus::Message & message) {
        return message.type() == sihd::sys::linux_::dbus::Message::Type::method_return
               || message.type() == sihd::sys::linux_::dbus::Message::Type::error;
    });
}

inline constexpr const char *portal_handle = "/org/freedesktop/portal/desktop/request/1_1/token";

inline std::string error_line(sihd::util::ErrorCode code, const std::string & message)
{
    return fmt::format("error {}: {}", static_cast<int>(code), message);
}

inline void refuse(sihd::sys::linux_::dbus::Connection & server,
                   const sihd::sys::linux_::dbus::Message & call,
                   const std::string & error_name)
{
    auto refusal = call.error_return(error_name, "denied");
    ASSERT_TRUE(refusal.has_value());
    ASSERT_TRUE(refusal->send(server).has_value());
}

inline void answer_u32_variant(sihd::sys::linux_::dbus::Connection & server,
                               const sihd::sys::linux_::dbus::Message & call,
                               uint32_t value)
{
    auto reply = call.method_return();
    ASSERT_TRUE(reply.has_value());
    sihd::sys::linux_::dbus::Appender body(*reply);
    auto variant = body.open_variant("u");
    ASSERT_TRUE(variant.has_value());
    ASSERT_TRUE(variant->add(value).has_value());
    ASSERT_TRUE(body.close(std::move(*variant)).has_value());
    ASSERT_TRUE(reply->send(server).has_value());
}

} // namespace test

#endif

#ifndef __SIHD_NET_TEST_HELPERS_HPP__
#define __SIHD_NET_TEST_HELPERS_HPP__

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/net/IpAddr.hpp>
#include <sihd/net/NetInterface.hpp>
#include <sihd/net/Socket.hpp>
#include <sihd/sys/PluginLoader.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/ALogger.hpp>
#include <sihd/util/LoggerFilter.hpp>
#include <sihd/util/LoggerManager.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/Waitable.hpp>
#include <sihd/util/Worker.hpp>
#include <sihd/util/build.hpp>
#include <sihd/util/thread.hpp>
#include <sihd/util/time.hpp>

namespace test
{

struct LoggedRecord
{
        sihd::util::LogLevel level;
        std::string source;
        std::string thread_name;
        sihd::util::Timestamp timestamp;
        std::string message;
};

class RemoteLogCapture: public sihd::util::ALogger
{
    public:
        static constexpr std::string_view remote_source = "test::remote";

        RemoteLogCapture(): ALogger("capture"), _test_thread(sihd::util::thread::id()) {}

        void log(const sihd::util::LogInfo & info, std::string_view msg) override
        {
            if (info.thread_id == _test_thread || info.source != remote_source)
                return;
            {
                auto lock = _waitable.guard();
                _records.push_back(
                    {info.level, std::string(info.source), info.thread_name, info.timestamp(), std::string(msg)});
            }
            _waitable.notify_all();
        }

        std::vector<LoggedRecord> wait_records(size_t count)
        {
            auto lock = _waitable.wait_for_guard(sihd::util::Duration(sihd::util::time::sec(5)),
                                                 [&] { return _records.size() >= count; });
            return _records;
        }

        std::vector<std::string> wait_messages(size_t count)
        {
            std::vector<std::string> messages;
            for (const LoggedRecord & record : this->wait_records(count))
                messages.push_back(record.message);
            return messages;
        }

    private:
        const pthread_t _test_thread;
        sihd::util::Waitable _waitable;
        std::vector<LoggedRecord> _records;
};

inline RemoteLogCapture *install_capture()
{
    auto *capture = new RemoteLogCapture();
    sihd::util::LoggerManager::add(capture);
    return capture;
}

// a writer installed next to a reader of the same process must not send the re-injected records again
template <typename Writer>
Writer *install_writer(const std::string & name)
{
    auto *writer = new Writer(name);
    sihd::util::LoggerFilter::Options options;
    options.thread_ne = sihd::util::thread::id();
    writer->add_filter(new sihd::util::LoggerFilter(options));
    sihd::util::LoggerManager::add(writer);
    return writer;
}

inline void expect_created_by_factory(const std::string & classname)
{
    if constexpr (sihd::util::build::is_statically_linked && !SIHD_FACTORY_TABLE)
        GTEST_SKIP() << "no factory resolution in this build";

    auto created = sihd::sys::PluginLoader::create_from_linked(classname, "by_factory");
    ASSERT_TRUE(created.has_value()) << created.error().message;
    EXPECT_EQ(created.value()->name(), "by_factory");
    delete created.value();
}

// sends 256 KiB at a time to a peer that never reads: true once a send fails
template <typename Sender>
bool send_until_failure(Sender & sender)
{
    const std::string payload(256 * 1024, 'x');
    for (int i = 0; i < 64; ++i)
    {
        if (!sender.send_all({payload.data(), payload.size()}))
            return true;
    }
    return false;
}

template <typename Reader>
class LoggerReaderScope
{
    public:
        LoggerReaderScope(): reader("logger-reader"), _worker([this] { return reader.start(); }) {}

        ~LoggerReaderScope() { this->stop(); }

        bool start()
        {
            return _worker.start_sync_worker("logger-reader")
                   && reader.wait_ready(sihd::util::Duration(sihd::util::time::sec(2)));
        }

        void stop()
        {
            if (reader.is_running())
                reader.stop();
            _worker.stop_worker();
        }

        Reader reader;

    private:
        sihd::util::Worker _worker;
};

template <typename Writer, typename Reader>
void expect_writer_reconnects(const std::string & path, RemoteLogCapture *capture)
{
    Writer writer("writer");
    const sihd::util::LogInfo info(RemoteLogCapture::remote_source, sihd::util::LogLevel::info);
    {
        LoggerReaderScope<Reader> first;
        ASSERT_TRUE(first.reader.set_unix_path(path));
        ASSERT_TRUE(first.start());
        ASSERT_TRUE(writer.set_unix_path(path));
        ASSERT_TRUE(writer.set_reconnect_interval(60000));
        writer.log(info, "before the restart");
        ASSERT_EQ(capture->wait_messages(1), (std::vector<std::string> {"before the restart"}));
    }

    testing::internal::CaptureStderr();
    writer.log(info, "lost");
    writer.log(info, "lost again");
    const std::string warned = testing::internal::GetCapturedStderr();
    EXPECT_FALSE(writer.is_connected());
    EXPECT_TRUE(warned.starts_with("writer: cannot reach " + path + ": ")) << warned;
    EXPECT_TRUE(warned.ends_with(", dropping messages until it is back\n")) << warned;
    EXPECT_EQ(std::count(warned.begin(), warned.end(), '\n'), 1) << warned;

    LoggerReaderScope<Reader> second;
    ASSERT_TRUE(second.reader.set_unix_path(path));
    ASSERT_TRUE(second.start());
    writer.log(info, "gated by the backoff");
    EXPECT_FALSE(writer.is_connected());

    ASSERT_TRUE(writer.set_reconnect_interval(0));
    writer.log(info, "after the restart");
    EXPECT_TRUE(writer.is_connected());
    EXPECT_EQ(capture->wait_messages(2), (std::vector<std::string> {"before the restart", "after the restart"}));
}

inline sihd::net::IpAddr bound_address(const sihd::net::Socket & socket)
{
    return sihd::net::IpAddr::localhost(socket.local_port().value_or(0));
}

// a loopback port free for tcp and udp right now, for a device that cannot bind port 0: use it at once
inline int free_loopback_port()
{
    // another process may hold the ephemeral tcp port in udp: draw another one
    for (int attempt = 0; attempt < 64; ++attempt)
    {
        sihd::net::Socket tcp;
        sihd::net::Socket udp;
        if (!tcp.open(AF_INET, SOCK_STREAM, IPPROTO_TCP) || !tcp.bind(sihd::net::IpAddr::localhost(0)))
            return 0;
        const int port = tcp.local_port().value_or(0);
        if (udp.open(AF_INET, SOCK_DGRAM, IPPROTO_UDP) && udp.bind(sihd::net::IpAddr::localhost(port)))
            return port;
    }
    return 0;
}

inline std::optional<sihd::net::NetInterface> find_interface(sihd::net::NetInterface::Kind kind, bool running)
{
    auto interfaces = sihd::net::NetInterface::get_all_interfaces();
    if (!interfaces)
        return std::nullopt;
    for (const auto & [name, netif] : *interfaces)
    {
        if (netif.kind() == kind && (!running || netif.running()))
            return netif;
    }
    return std::nullopt;
}

} // namespace test

#if !defined(__SIHD_WINDOWS__)

# include <sys/socket.h>

# include <csignal>

# include <sihd/net/Socket.hpp>
# include <sihd/sys/os.hpp>

namespace test
{

class ScopedSignalAction
{
    public:
        ScopedSignalAction(int signo, void (*handler)(int)): _signo(signo)
        {
            struct sigaction action = {};
            action.sa_handler = handler;
            _installed = sigaction(signo, &action, &_previous) == 0;
        }

        ~ScopedSignalAction()
        {
            if (_installed)
                sigaction(_signo, &_previous, nullptr);
        }

        ScopedSignalAction(const ScopedSignalAction &) = delete;
        ScopedSignalAction & operator=(const ScopedSignalAction &) = delete;

        bool installed() const { return _installed; }

    private:
        int _signo;
        bool _installed = false;
        struct sigaction _previous = {};
};

// close() then sends an RST instead of a FIN
inline bool set_reset_on_close(const sihd::net::Socket & sock)
{
    const struct linger abortive = {.l_onoff = 1, .l_linger = 0};
    return sihd::sys::os::setsockopt(sock.socket(), SOL_SOCKET, SO_LINGER, &abortive, sizeof(abortive)).has_value();
}

} // namespace test

#endif

#endif

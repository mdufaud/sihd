#include <array>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <future>
#include <map>
#include <optional>
#include <string>
#include <thread>

#include <sihd/util/build.hpp>

#if defined(__SIHD_WINDOWS__)
# include <windows.h>
#endif

#include <sihd/sys/CrashHandler.hpp>
#include <sihd/sys/Daemon.hpp>
#include <sihd/sys/DesktopNotifier.hpp>
#include <sihd/sys/MessageQueue.hpp>
#include <sihd/sys/PowerInhibitor.hpp>
#include <sihd/sys/SecretStore.hpp>
#include <sihd/sys/Semaphore.hpp>
#include <sihd/sys/SharedMemory.hpp>
#include <sihd/sys/SleepWatcher.hpp>
#include <sihd/sys/desktop.hpp>
#include <sihd/sys/network.hpp>
#include <sihd/util/time.hpp>

using namespace sihd::util;

namespace
{
struct shmbuf
{
        int data[2];
};

int do_sleep(const char *seconds)
{
    std::this_thread::sleep_for(std::chrono::seconds(std::stoi(seconds)));
    return 0;
}

int do_shm(const char *id)
{
    sihd::sys::SharedMemory mem;
    if (!mem.attach(id, sizeof(shmbuf)) || mem.data() == nullptr)
        return 0;

    struct shmbuf *shmp = static_cast<struct shmbuf *>(mem.data());
    int ret = 1 << 1;

    if (shmp->data[0] == 42 && shmp->data[1] == 24)
        ret += 1 << 2;

    shmp->data[0] = 7;
    shmp->data[1] = 8;

    return ret;
}

int do_sem_wait(const char *name)
{
    sihd::sys::Semaphore sem;
    if (!sem.attach(name))
        return 0;

    int ret = 1 << 1;
    if (!sem.wait())
        return ret;

    return ret + (1 << 2);
}

int do_sem_post(const char *name)
{
    sihd::sys::Semaphore sem;
    if (!sem.attach(name))
        return 0;

    if (!sem.post())
        return 1 << 1;

    return (1 << 1) + (1 << 2);
}

// the peer may be re-arming its pipe: retry the open a bounded time
std::optional<sihd::sys::MessageQueue> open_mq(const char *name)
{
    sihd::sys::MessageQueue mq;
    auto opened = mq.open(name);
    for (int i = 0; !opened && i < 30; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        opened = mq.open(name);
    }
    if (!opened)
    {
        fprintf(stderr, "mq open error: %s\n", opened.error().message.c_str());
        return std::nullopt;
    }
    return mq;
}

int do_mq_send(const char *name, const char *payload)
{
    auto mq = open_mq(name);
    if (!mq)
        return 0;

    int ret = 1 << 1;
    auto sent = mq->send(ArrByteView(payload, strlen(payload)));
    if (!sent)
    {
        fprintf(stderr, "mq send error: %s\n", sent.error().message.c_str());
        return ret;
    }

    return ret + (1 << 2);
}

int do_mq_receive(const char *name, const char *expected)
{
    auto mq = open_mq(name);
    if (!mq)
        return 0;

    auto message = mq->receive();
    if (!message)
    {
        fprintf(stderr, "mq receive error: %s\n", message.error().message.c_str());
        return 1 << 1;
    }

    int ret = 1 << 1;
    if (message->size() == strlen(expected) && memcmp(message->data(), expected, message->size()) == 0)
        ret += 1 << 2;

    return ret;
}

int do_daemonize(const char *pid_file, const char *working_dir, const char *seconds)
{
    try
    {
        sihd::sys::Daemon daemon("helper-daemon");
        if (!daemon.set_pid_file_path(pid_file))
            return 1;
        if (working_dir[0] != '\0' && !daemon.set_working_dir_path(working_dir))
            return 1;
        if (!daemon.run())
            return 1;
        std::this_thread::sleep_for(std::chrono::seconds(std::stoi(seconds)));
        return 0;
    }
    catch (const std::exception &)
    {
        return 2;
    }
}

std::map<std::string, std::string> named_args(int argc, char **argv)
{
    std::map<std::string, std::string> args;
    for (int i = 0; i < argc; ++i)
    {
        const std::string arg = argv[i];
        const size_t equal = arg.find('=');
        if (equal != std::string::npos)
            args[arg.substr(0, equal)] = arg.substr(equal + 1);
    }
    return args;
}

// prints one line per attempt: the id or "error: <message>", exits 0 when the last attempt succeeded
int do_notify(int argc, char **argv)
{
    std::map<std::string, std::string> args = named_args(argc, argv);
    sihd::sys::DesktopNotifier notifier;
    notifier.set_app_name(args["app_name"]);
    notifier.set_app_id(args["app_id"]);
    sihd::sys::DesktopNotifier::Notification notification;
    notification.summary = args["summary"];
    notification.body = args["body"];
    notification.icon = args["icon"];
    if (!args["urgency"].empty())
        notification.urgency = static_cast<sihd::sys::DesktopNotifier::Urgency>(std::stoi(args["urgency"]));
    if (!args["timeout_ms"].empty())
        notification.timeout = Duration(time::milli(std::stoll(args["timeout_ms"])));
    const int attempts = args["attempts"].empty() ? 1 : std::stoi(args["attempts"]);

    bool last_succeeded = false;
    for (int attempt = 0; attempt < attempts; ++attempt)
    {
        auto id = notifier.notify(notification);
        last_succeeded = id.has_value();
        if (last_succeeded)
            printf("%u\n", *id);
        else
            printf("error: %s\n", id.error().message.c_str());
    }
    return last_succeeded ? 0 : 1;
}

int do_notify_close(int argc, char **argv)
{
    std::map<std::string, std::string> args = named_args(argc, argv);
    sihd::sys::DesktopNotifier notifier;
    auto closed = notifier.close(static_cast<uint32_t>(std::stoul(args["id"])));
    if (!closed)
    {
        printf("error: %s\n", closed.error().message.c_str());
        return 1;
    }
    return 0;
}

void wait_proceed()
{
    fflush(stdout);
    std::array<char, 8> line {};
    if (fgets(line.data(), static_cast<int>(line.size()), stdin) == nullptr)
        return;
}

void print_error(const Error & error)
{
    printf("error %d: %s\n", static_cast<int>(error.code), error.message.c_str());
}

void print_result(const Result<void> & result, const char *success)
{
    if (result.has_value())
        printf("%s\n", success);
    else
        print_error(result.error());
}

Result<void> power_inhibit(sihd::sys::PowerInhibitor & inhibitor, const std::string & what, const std::string & reason)
{
    return what == "display" ? inhibitor.inhibit_display(reason) : inhibitor.inhibit_sleep(reason);
}

int do_power_inhibit(int argc, char **argv)
{
    std::map<std::string, std::string> args = named_args(argc, argv);
    std::optional<sihd::sys::PowerInhibitor> inhibitor;
    inhibitor.emplace();
    inhibitor->set_app_name(args["app_name"]);
    auto inhibited = power_inhibit(*inhibitor, args["what"], args["reason"]);
    print_result(inhibited, "held");
    if (!inhibited.has_value())
    {
        printf("inhibited=%d\n", inhibitor->is_sleep_inhibited() || inhibitor->is_display_inhibited() ? 1 : 0);
        fflush(stdout);
        return 1;
    }

    if (args["twice"] == "1")
        print_result(power_inhibit(*inhibitor, args["what"], args["reason"]), "held");
    wait_proceed();
    if (args["end"] == "destroy")
    {
        inhibitor.reset();
        printf("released\n");
    }
    else
    {
        print_result(inhibitor->release(), "released");
    }
    wait_proceed();
    return 0;
}

int do_sleep_watch(int argc, char **argv)
{
    std::map<std::string, std::string> args = named_args(argc, argv);
    sihd::sys::SleepWatcher watcher;
    watcher.set_app_name(args["app_name"]);
    std::promise<void> resumed;
    const sihd::sys::SleepWatcher::Callback callback = [&resumed](sihd::sys::SleepWatcher::Event event) {
        if (event == sihd::sys::SleepWatcher::Event::suspending)
        {
            printf("suspending\n");
            wait_proceed();
        }
        else
        {
            printf("resumed\n");
            fflush(stdout);
            resumed.set_value();
        }
    };

    auto started = watcher.start(callback);
    print_result(started, "started");
    if (!started.has_value())
    {
        printf("running=%d\n", watcher.is_running() ? 1 : 0);
        fflush(stdout);
        return 1;
    }
    if (args["twice"] == "1")
        print_result(watcher.start(callback), "started");
    fflush(stdout);
    resumed.get_future().wait();
    print_result(watcher.stop(), "stopped");
    wait_proceed();
    return 0;
}

const char *color_scheme_name(sihd::sys::desktop::ColorScheme scheme)
{
    switch (scheme)
    {
        case sihd::sys::desktop::ColorScheme::dark:
            return "dark";
        case sihd::sys::desktop::ColorScheme::light:
            return "light";
        default:
            return "no_preference";
    }
}

int do_desktop(int argc, char **argv)
{
    std::map<std::string, std::string> args = named_args(argc, argv);
    Result<void> result;
    if (args["action"] == "color_scheme")
    {
        auto scheme = sihd::sys::desktop::color_scheme();
        if (scheme.has_value())
            printf("scheme=%s\n", color_scheme_name(*scheme));
        else
            print_error(scheme.error());
        fflush(stdout);
        wait_proceed();
        return scheme.has_value() ? 0 : 1;
    }

    if (args["action"] == "open_uri")
        result = sihd::sys::desktop::open_uri(args["target"]);
    else if (args["action"] == "open_file")
        result = sihd::sys::desktop::open_file(args["target"]);
    else
        result = sihd::sys::desktop::reveal(args["target"]);
    print_result(result, "ok");
    wait_proceed();
    return result.has_value() ? 0 : 1;
}

const char *connectivity_name(sihd::sys::network::Connectivity connectivity)
{
    switch (connectivity)
    {
        case sihd::sys::network::Connectivity::none:
            return "none";
        case sihd::sys::network::Connectivity::portal:
            return "portal";
        case sihd::sys::network::Connectivity::limited:
            return "limited";
        case sihd::sys::network::Connectivity::full:
            return "full";
        default:
            return "unknown";
    }
}

int do_connectivity()
{
    auto connectivity = sihd::sys::network::connectivity();
    if (connectivity.has_value())
        printf("%s\n", connectivity_name(*connectivity));
    else
        print_error(connectivity.error());
    wait_proceed();
    return connectivity.has_value() ? 0 : 1;
}

int do_secret(int argc, char **argv)
{
    std::map<std::string, std::string> args = named_args(argc, argv);
    sihd::sys::SecretStore store;
    if (!args["prompt_ms"].empty())
        store.set_prompt_timeout(Duration(time::milli(std::stoll(args["prompt_ms"]))));
    std::optional<Error> error;
    if (args["action"] == "lookup")
    {
        auto secret = store.lookup(args["service"], args["account"]);
        if (secret.has_value())
            printf("secret=%s\n", secret->c_str());
        else
            error = secret.error();
    }
    else
    {
        auto done = args["action"] == "store" ? store.store(args["service"], args["account"], args["secret"])
                                              : store.remove(args["service"], args["account"]);
        if (done.has_value())
            printf("ok\n");
        else
            error = done.error();
    }

    if (error.has_value())
        print_error(*error);
    wait_proceed();
    return error.has_value() ? 1 : 0;
}

int do_crash(const char *dump_path)
{
    sihd::sys::CrashHandler::Options options;
    options.dump_path = dump_path;
    // rethrow would trigger the system core dump machinery, which a test must not depend on
    options.rethrow = false;
    sihd::sys::CrashHandler handler(options);
#if defined(__SIHD_WINDOWS__)
    RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr);
#else
    std::raise(SIGSEGV);
#endif
    return 0;
}
} // namespace

int main(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "args") == 0)
    {
        for (int i = 2; i < argc; ++i)
            printf("%s\n", argv[i]);
        return 0;
    }
    if (argc >= 3 && strcmp(argv[1], "sleep") == 0)
        return do_sleep(argv[2]);
    if (argc >= 3 && strcmp(argv[1], "shm") == 0)
        return do_shm(argv[2]);
    if (argc >= 3 && strcmp(argv[1], "sem-wait") == 0)
        return do_sem_wait(argv[2]);
    if (argc >= 3 && strcmp(argv[1], "sem-post") == 0)
        return do_sem_post(argv[2]);
    if (argc >= 4 && strcmp(argv[1], "mq-send") == 0)
        return do_mq_send(argv[2], argv[3]);
    if (argc >= 4 && strcmp(argv[1], "mq-receive") == 0)
        return do_mq_receive(argv[2], argv[3]);
    if (argc >= 3 && strcmp(argv[1], "crash") == 0)
        return do_crash(argv[2]);
    if (argc >= 2 && strcmp(argv[1], "notify") == 0)
        return do_notify(argc - 2, argv + 2);
    if (argc >= 2 && strcmp(argv[1], "notify-close") == 0)
        return do_notify_close(argc - 2, argv + 2);
    if (argc >= 2 && strcmp(argv[1], "power-inhibit") == 0)
        return do_power_inhibit(argc - 2, argv + 2);
    if (argc >= 2 && strcmp(argv[1], "sleep-watch") == 0)
        return do_sleep_watch(argc - 2, argv + 2);
    if (argc >= 2 && strcmp(argv[1], "desktop") == 0)
        return do_desktop(argc - 2, argv + 2);
    if (argc >= 2 && strcmp(argv[1], "connectivity") == 0)
        return do_connectivity();
    if (argc >= 2 && strcmp(argv[1], "secret") == 0)
        return do_secret(argc - 2, argv + 2);
    if (argc >= 5 && strcmp(argv[1], "daemonize") == 0)
        return do_daemonize(argv[2], argv[3], argv[4]);
    return 255;
}

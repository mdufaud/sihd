#include <chrono>
#include <cstring>
#include <string>
#include <thread>

#include <sihd/sys/Daemon.hpp>
#include <sihd/sys/SharedMemory.hpp>

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
} // namespace

int main(int argc, char **argv)
{
    if (argc >= 3 && strcmp(argv[1], "sleep") == 0)
        return do_sleep(argv[2]);
    if (argc >= 3 && strcmp(argv[1], "shm") == 0)
        return do_shm(argv[2]);
    if (argc >= 5 && strcmp(argv[1], "daemonize") == 0)
        return do_daemonize(argv[2], argv[3], argv[4]);
    return 255;
}

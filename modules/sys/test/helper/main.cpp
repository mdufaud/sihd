#include <chrono>
#include <cstring>
#include <string>
#include <thread>

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
} // namespace

int main(int argc, char **argv)
{
    if (argc >= 3 && strcmp(argv[1], "sleep") == 0)
        return do_sleep(argv[2]);
    if (argc >= 3 && strcmp(argv[1], "shm") == 0)
        return do_shm(argv[2]);
    return 255;
}

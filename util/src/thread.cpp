#include <cstring>
#include <map>
#include <mutex>
#include <sstream>

#include <sihd/util/str.hpp>
#include <sihd/util/thread.hpp>

namespace sihd::util::thread
{

namespace
{

struct ComparePthreadKeys
{
        bool operator()(const pthread_t & id1, const pthread_t & id2) const
        {
            return memcmp(&id1, &id2, sizeof(pthread_t)) < 0;
        }
};

pthread_t g_main_thread_id = id();
std::mutex g_thread_mutex;
std::map<pthread_t, std::string, ComparePthreadKeys> g_thread_map = {{g_main_thread_id, "main"}};

} // namespace

pthread_t main()
{
    return g_main_thread_id;
}

bool equals(const pthread_t & id1, const pthread_t & id2)
{
    return memcmp(&id1, &id2, sizeof(pthread_t)) == 0;
}

pthread_t id()
{
    return pthread_self();
}

std::string id_str(pthread_t id)
{
    return "0x" + str::to_hex(id);
}

void set_name(const std::string & name)
{
    std::lock_guard lock(g_thread_mutex);
    g_thread_map[id()] = name;
}

void del_name()
{
    std::lock_guard lock(g_thread_mutex);
    g_thread_map.erase(id());
}

const std::string & name()
{
    std::lock_guard lock(g_thread_mutex);
    return g_thread_map[id()];
}

} // namespace sihd::util::thread

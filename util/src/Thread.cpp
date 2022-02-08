#include <sihd/util/Thread.hpp>
#include <sihd/util/Str.hpp>
#include <string>
#include <sstream>

namespace sihd::util
{

pthread_t Thread::main = Thread::id();
std::mutex Thread::thread_mutex;
std::map<pthread_t, std::string, Thread::ComparePthreadKeys> Thread::thread_map = {
    {Thread::main, "main"}
};

bool    Thread::equals(const pthread_t & id1, const pthread_t & id2)
{
    return memcmp(&id1, &id2, sizeof(pthread_t)) == 0;
}

pthread_t   Thread::id()
{
    return pthread_self();
}

std::string     Thread::id_str(pthread_t id)
{
    return "0x" + Str::to_hex(id);
}

std::string     Thread::id_str()
{
    return Thread::id_str(Thread::id());
}

void    Thread::set_name(const std::string & name)
{
    std::lock_guard lock(thread_mutex);
    thread_map[Thread::id()] = name;
}

void    Thread::del_name()
{
    std::lock_guard lock(thread_mutex);
    thread_map.erase(Thread::id());
}

const std::string & Thread::get_name()
{
    std::lock_guard lock(thread_mutex);
    return thread_map[Thread::id()];
}

}
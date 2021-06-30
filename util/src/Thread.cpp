#include <sihd/util/Thread.hpp>
#include <sstream>

namespace sihd::util
{

std::thread::id Thread::main = id();
std::mutex      Thread::thread_mutex;
std::map<std::thread::id, std::string>  Thread::thread_map = {
    {Thread::main, "main"}
};

std::thread::id     Thread::id()
{
    return std::this_thread::get_id();
}

std::string     Thread::id_str(std::thread::id id)
{
    std::stringstream ss;
    ss << "0x" << std::hex << id;
    return ss.str();
}

std::string     Thread::id_str()
{
    return id_str(id());
}

void    Thread::set_name(const std::string & name)
{
    std::lock_guard lock(thread_mutex);
    thread_map[id()] = name;
}

void    Thread::del_name()
{
    std::lock_guard lock(thread_mutex);
    thread_map.erase(id());
}

const std::string & Thread::get_name()
{
    std::lock_guard lock(thread_mutex);
    return thread_map[id()];
}

}
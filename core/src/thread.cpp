#include <sihd/core/thread.hpp>
#include <sstream>

namespace sihd::core::thread
{

std::thread::id main = id();

std::map<std::thread::id, std::string>  thread_map = {
    {main, "main"}
};

std::thread::id id()
{
    return std::this_thread::get_id();
}

std::string id_str(std::thread::id id)
{
    std::stringstream ss;
    ss << "0x" << std::hex << id;
    return ss.str();
}

std::string id_str()
{
    return id_str(id());
}

void    set_name(const std::string & name)
{
    thread_map[id()] = name;
}

void    del_name()
{
    thread_map.erase(id());
}

const std::string & get_name()
{
    return thread_map[id()];
}

}
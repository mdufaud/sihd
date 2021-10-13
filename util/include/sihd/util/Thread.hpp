#ifndef __SIHD_UTIL_THREAD_HPP__
# define __SIHD_UTIL_THREAD_HPP__

# include <thread>
# include <string>
# include <map>
# include <mutex>

namespace sihd::util
{

class Thread
{
    public:
        static std::thread::id id();
        static std::string id_str();
        static std::string id_str(std::thread::id id);
        static void set_name(const std::string & name);
        static void del_name();
        static const std::string & get_name();

        static std::thread::id main;

    private:
        Thread() {};
        ~Thread() {};

        static std::mutex thread_mutex;
        static std::map<std::thread::id, std::string> thread_map;

};

}

#endif 
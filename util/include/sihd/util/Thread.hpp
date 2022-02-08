#ifndef __SIHD_UTIL_THREAD_HPP__
# define __SIHD_UTIL_THREAD_HPP__

# include <thread>
# include <string>
# include <map>
# include <mutex>

# include <string.h>

namespace sihd::util
{

class Thread
{
    public:
        static pthread_t id();
        static std::string id_str();
        static std::string id_str(pthread_t id);
        static void set_name(const std::string & name);
        static void del_name();
        static const std::string & get_name();
        static bool equals(const pthread_t & id1, const pthread_t & id2);

        static pthread_t main;

    private:
        struct ComparePthreadKeys
        {
            bool operator()(const pthread_t & id1, const pthread_t & id2) const
            {
                return memcmp(&id1, &id2, sizeof(pthread_t)) < 0;
            }
        };

        Thread() {};
        ~Thread() {};

        static std::mutex thread_mutex;
        static std::map<pthread_t, std::string, ComparePthreadKeys> thread_map;

};

}

#endif
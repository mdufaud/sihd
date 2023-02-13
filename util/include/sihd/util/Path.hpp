#ifndef __SIHD_UTIL_PATH_HPP__
# define __SIHD_UTIL_PATH_HPP__

# include <sihd/util/str.hpp>
# include <map>
# include <mutex>

namespace sihd::util
{

class Path
{
    private:
        Path() {};
        ~Path() {};

        static std::mutex path_mutex;
        static std::map<std::string, std::string> url_to_path;

    public:
        static void clear_all();
        static void clear(const std::string & url);
        static void set(const std::string & url, const std::string & path);
        static std::string get(const std::string & path);
        static std::string get(const std::string & url, const std::string & path);
        static std::string get_from(const std::string & from, const std::string & path);
        static std::string find(const std::string & path);

        static std::string url_delimiter;
};

}

#endif
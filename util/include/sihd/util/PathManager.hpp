#ifndef __SIHD_UTIL_PATH_HPP__
#define __SIHD_UTIL_PATH_HPP__

#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sihd::util
{

class PathManager
{
    public:
        PathManager();
        ~PathManager() = default;

        std::vector<std::string> find_all(std::string_view search, std::string_view mode_str = "") const;
        std::string find(std::string_view search, std::string_view mode_str = "") const;

        void push_back(std::string_view path_item);
        void push_front(std::string_view path_item);
        bool remove(std::string_view path_item);
        void clear();

        const std::vector<std::string> & path_lst() const { return _path_lst; }

    private:
        std::vector<std::string> _path_lst;
        mutable std::mutex _mutex;
};

} // namespace sihd::util

#endif
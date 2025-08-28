#include <algorithm>
#include <filesystem>

#include <sihd/util/PathManager.hpp>
#include <sihd/util/Splitter.hpp>
#include <sihd/util/container.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/platform.hpp>

namespace sihd::util
{

namespace
{

std::filesystem::perms mode_from_str(std::string_view mode)
{
    std::filesystem::perms ret = std::filesystem::perms::none;

    size_t i = 0;
    while (i < mode.size())
    {
        if (mode[i] == 'r')
            ret &= std::filesystem::perms::owner_read;
        else if (mode[i] == 'w')
            ret &= std::filesystem::perms::owner_write;
        else if (mode[i] == 'x')
            ret &= std::filesystem::perms::owner_exec;
        ++i;
    }

    return ret;
}

bool check_if_match(std::string_view path,
                    std::string_view search,
                    std::string_view mode_str,
                    std::string & result_str)
{
    bool ret = false;
    std::filesystem::perms mode = mode_from_str(mode_str);
    std::string path_to_search = fs::combine(path, search);
    if (fs::exists(path_to_search))
    {
        if (mode != std::filesystem::perms::none)
        {
            std::error_code ec;
            std::filesystem::perms file_perms = std::filesystem::status(path_to_search, ec).permissions();
            ret = !ec && (file_perms & mode) == std::filesystem::perms::none;
        }
        else
        {
            ret = true;
        }
    }
    if (ret)
    {
        result_str = std::move(path_to_search);
    }
    return ret;
}

} // namespace

PathManager::PathManager() = default;

std::vector<std::string> PathManager::find_all(std::string_view search, std::string_view mode_str) const
{
    std::vector<std::string> ret;
    std::lock_guard lock(_mutex);
    for (const std::string & path : _path_lst)
    {
        std::string result_str;
        if (check_if_match(path, search, mode_str, result_str))
            ret.emplace_back(std::move(result_str));
    }
    return ret;
}

std::string PathManager::find(std::string_view search, std::string_view mode_str) const
{
    std::lock_guard lock(_mutex);
    std::string result_str;
    for (const auto & path : _path_lst)
    {
        if (check_if_match(path, search, mode_str, result_str))
            return result_str;
    }
    return "";
}

void PathManager::push_back(std::string_view path_item)
{
    if (!container::contains(_path_lst, path_item))
        _path_lst.emplace_back(path_item);
}

void PathManager::push_front(std::string_view path_item)
{
    if (!container::contains(_path_lst, path_item))
        _path_lst.emplace(_path_lst.begin(), path_item);
}

bool PathManager::remove(std::string_view path_item)
{
    auto it = std::remove(_path_lst.begin(), _path_lst.end(), path_item);
    if (it == _path_lst.end())
        return false;
    _path_lst.erase(it);
    return true;
}

void PathManager::clear()
{
    _path_lst.clear();
}

} // namespace sihd::util
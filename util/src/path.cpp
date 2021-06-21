#include <sihd/util/path.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::util::path
{

using namespace std::experimental;

std::mutex  _path_mutex;
std::map<std::string, std::string> _uri_to_path;

void    clear_all()
{
    std::lock_guard lock(_path_mutex);
    _uri_to_path.clear();
}

void    clear(const std::string & uri)
{
    std::lock_guard lock(_path_mutex);
    _uri_to_path.erase(uri);
}

void    set(const std::string & uri, const std::string & path)
{
    std::lock_guard lock(_path_mutex);
    _uri_to_path[uri] = path;
}

std::string get(const std::string & req)
{
    std::vector<std::string> split = sihd::util::str::split(req, "://");
    if (split.size() <= 1)
        return get_from("", req);
    return get(split[0], split[1]);
}

std::string get(const std::string & uri, const std::string & path)
{
    std::string uri_path;
    {
        std::lock_guard lock(_path_mutex);
        uri_path = _uri_to_path[uri];
    }
    return get_from(uri_path, path);
}

std::string get_from(const std::string & from, const std::string & path)
{
    filesystem::path fs_path(from);
    fs_path.append(path);
    if (filesystem::exists(fs_path))
        return fs_path.c_str();
    return "";
}

std::string find(const std::string & path)
{
    std::lock_guard lock(_path_mutex);
    std::string ret;
    for (const auto & [uri, uri_path] : _uri_to_path)
    {
        (void)uri;
        ret = get_from(uri_path, path);
        if (!ret.empty())
            break ;
    }
    return ret;
}

}
#include <sihd/util/Path.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::util
{

using namespace std::experimental;

std::string Path::url_delimiter = "://";
std::mutex  Path::path_mutex;
std::map<std::string, std::string> Path::url_to_path;

void    Path::clear_all()
{
    std::lock_guard lock(path_mutex);
    url_to_path.clear();
}

void    Path::clear(const std::string & url)
{
    std::lock_guard lock(path_mutex);
    url_to_path.erase(url);
}

void    Path::set(const std::string & url, const std::string & path)
{
    std::lock_guard lock(path_mutex);
    url_to_path[url] = path;
}

std::string     Path::get(const std::string & req)
{
    std::vector<std::string> split = Str::split(req, url_delimiter.c_str());
    if (split.size() <= 1)
        return get_from("", req);
    return get(split[0], split[1]);
}

std::string     Path::get(const std::string & url, const std::string & path)
{
    std::string url_path;
    {
        std::lock_guard lock(path_mutex);
        url_path = url_to_path[url];
    }
    return get_from(url_path, path);
}

std::string     Path::get_from(const std::string & from, const std::string & path)
{
    filesystem::path fs_path(from);
    fs_path.append(path);
    if (filesystem::exists(fs_path))
        return fs_path.generic_string();
    return "";
}

std::string     Path::find(const std::string & path)
{
    std::lock_guard lock(path_mutex);
    std::string ret;
    for (const auto & [url, url_path] : url_to_path)
    {
        (void)url;
        ret = get_from(url_path, path);
        if (!ret.empty())
            break ;
    }
    return ret;
}

}
#include <sihd/util/Path.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/Splitter.hpp>

namespace sihd::util
{

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
    Splitter splitter(url_delimiter);
    std::vector<std::string> split = splitter.split(req);
    if (split.size() <= 1)
        return Path::get_from("", req);
    return Path::get(split[0], split[1]);
}

std::string     Path::get(const std::string & url, const std::string & path)
{
    std::string url_path;
    {
        std::lock_guard lock(path_mutex);
        url_path = url_to_path[url];
    }
    return Path::get_from(url_path, path);
}

std::string     Path::get_from(const std::string & from, const std::string & path)
{
    std::string ret = fs::combine(from, path);
    if (fs::exists(ret))
        return ret;
    return "";
}

std::string     Path::find(const std::string & path)
{
    std::lock_guard lock(path_mutex);
    std::string ret;
    for (const auto & [url, url_path] : url_to_path)
    {
        (void)url;
        ret = Path::get_from(url_path, path);
        if (!ret.empty())
            break ;
    }
    return ret;
}

}
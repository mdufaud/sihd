#include <map>
#include <mutex>

#include <sihd/util/Splitter.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/path.hpp>

namespace sihd::util::path
{

namespace
{

std::string g_url_delimiter = "://";
std::mutex g_path_mutex;
std::map<std::string, std::string> g_url_to_path;

} // namespace

std::string url_delimiter()
{
    return g_url_delimiter;
}

void clear_all()
{
    std::lock_guard lock(g_path_mutex);
    g_url_to_path.clear();
}

void clear(const std::string & url)
{
    std::lock_guard lock(g_path_mutex);
    g_url_to_path.erase(url);
}

void set(const std::string & url, const std::string & path)
{
    std::lock_guard lock(g_path_mutex);
    g_url_to_path[url] = path;
}

std::string get(const std::string & req)
{
    Splitter splitter(g_url_delimiter);
    std::vector<std::string> split = splitter.split(req);
    if (split.size() <= 1)
        return get_from("", req);
    return get(split[0], split[1]);
}

std::string get(const std::string & url, const std::string & path)
{
    std::string url_path;
    {
        std::lock_guard lock(g_path_mutex);
        url_path = g_url_to_path[url];
    }
    return get_from(url_path, path);
}

std::string get_from(const std::string & from, const std::string & path)
{
    std::string ret = fs::combine(from, path);
    if (fs::exists(ret))
        return ret;
    return "";
}

std::string find(const std::string & path)
{
    std::lock_guard lock(g_path_mutex);
    std::string ret;
    for (const auto & [url, url_path] : g_url_to_path)
    {
        (void)url;
        ret = get_from(url_path, path);
        if (!ret.empty())
            break;
    }
    return ret;
}

} // namespace sihd::util::path
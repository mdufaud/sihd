#include <algorithm>
#include <vector>

#include <fmt/format.h>

#include <sihd/util/FilePoller.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/container.hpp>
#include <sihd/util/fs.hpp>

namespace sihd::util
{

SIHD_LOGGER;

namespace
{

struct FileStat
{
        std::string path;
        Timestamp last_accessed;
};

void map_path_into(std::vector<FileStat> & file_stats, const std::string & path, uint32_t max_depth)
{
    file_stats.clear();

    if (fs::is_file(path) || !fs::is_dir(path))
        return;

    auto files = fs::recursive_children(path, max_depth);
    std::sort(files.begin(), files.end());

    time_t last_write;
    file_stats.reserve(files.size());
    for (std::string & file : files)
    {
        last_write = fs::last_write(file);
        file_stats.emplace_back(FileStat {.path = std::move(file), .last_accessed = last_write});
    }
}

void put_intersections_into(std::vector<FileStat> & intersections,
                            const std::vector<FileStat> & old_files,
                            const std::vector<FileStat> & new_files)
{
    intersections.clear();
    intersections.reserve(std::min(old_files.size(), new_files.size()) + 1);

    std::set_intersection(old_files.begin(),
                          old_files.end(),
                          new_files.begin(),
                          new_files.end(),
                          std::back_inserter(intersections),
                          [](const FileStat & a, const FileStat & b) { return a.path < b.path; });
}

void put_differences_into(std::vector<FileStat> & differences,
                          const std::vector<FileStat> & old_files,
                          const std::vector<FileStat> & new_files)
{
    differences.clear();
    differences.reserve(std::abs((ssize_t)old_files.size() - (ssize_t)new_files.size()) + 1);

    std::set_difference(old_files.begin(),
                        old_files.end(),
                        new_files.begin(),
                        new_files.end(),
                        std::back_inserter(differences),
                        [](const FileStat & a, const FileStat & b) { return a.path < b.path; });
}

} // namespace

struct FilePoller::Impl
{
        std::string watch_path;
        size_t max_depth;
        bool path_exists;
        time_t last_modif;
        std::vector<FileStat> last;

        std::vector<std::string> created;
        std::vector<std::string> removed;
        std::vector<std::string> changed;

        std::vector<FileStat> __same_files;
        std::vector<FileStat> __created_files;
        std::vector<FileStat> __removed_files;
};

FilePoller::FilePoller()
{
    _impl = std::make_unique<Impl>();
}

FilePoller::FilePoller(std::string_view path, size_t max_depth): FilePoller()
{
    if (!this->watch(path, max_depth))
        throw std::runtime_error(fmt::format("cannot watch path '{}'", path));
}

FilePoller::~FilePoller()
{
    this->unwatch();
}

const std::vector<std::string> & FilePoller::created() const
{
    return _impl->created;
}

const std::vector<std::string> & FilePoller::removed() const
{
    return _impl->removed;
}

const std::vector<std::string> & FilePoller::changed() const
{
    return _impl->changed;
}

const std::string & FilePoller::watch_path() const
{
    return _impl->watch_path;
}

size_t FilePoller::watch_size() const
{
    return _impl->last.size() + (size_t)_impl->path_exists;
}

void FilePoller::unwatch()
{
    _impl = std::make_unique<Impl>();
}

bool FilePoller::watch(std::string_view path, size_t max_depth)
{
    if (_impl->watch_path == path)
        return true;

    this->unwatch();

    if (path.empty())
        return false;

    _impl->watch_path = path;
    _impl->max_depth = max_depth;

    _impl->path_exists = fs::exists(path);
    _impl->last_modif = _impl->path_exists ? Timestamp {} : fs::last_write(path);

    map_path_into(_impl->last, _impl->watch_path, max_depth);

    return true;
}

bool FilePoller::run()
{
    if (_impl->watch_path.empty())
        return false;

    const bool new_path_exists = fs::exists(_impl->watch_path);
    const time_t new_last_modif = fs::last_write(_impl->watch_path);

    std::vector<FileStat> new_files;
    new_files.reserve(_impl->last.size());
    map_path_into(new_files, _impl->watch_path, _impl->max_depth);

    _impl->created.clear();
    _impl->removed.clear();
    _impl->changed.clear();

    if (fs::is_dir(_impl->watch_path))
    {
        put_intersections_into(_impl->__same_files, _impl->last, new_files);

        // build changed files list
        auto old_files_it = _impl->last.begin();
        const auto old_files_end = _impl->last.end();
        auto same_files_it = _impl->__same_files.begin();
        const auto same_files_end = _impl->__same_files.end();
        while (same_files_it != same_files_end)
        {
            while (old_files_it->path != same_files_it->path && old_files_it != old_files_end)
                ++old_files_it;

            if (old_files_it == old_files_end)
                break;

            if (old_files_it->last_accessed != fs::last_write(same_files_it->path))
                _impl->changed.emplace_back(old_files_it->path);

            ++same_files_it;
        }

        // build created files list
        put_differences_into(_impl->__created_files, new_files, _impl->__same_files);
        for (auto & file : _impl->__created_files)
        {
            _impl->created.emplace_back(std::move(file.path));
        }

        // build removed files list
        put_differences_into(_impl->__removed_files, _impl->last, new_files);
        for (auto & file : _impl->__removed_files)
        {
            _impl->removed.emplace_back(std::move(file.path));
        }
    }

    // create changes on the exact watch dir or file
    if (!new_path_exists && _impl->path_exists)
        _impl->removed.emplace_back(_impl->watch_path);
    else if (new_path_exists && !_impl->path_exists)
        _impl->created.emplace_back(_impl->watch_path);
    else if (_impl->last_modif < new_last_modif)
        _impl->changed.emplace_back(_impl->watch_path);

    _impl->path_exists = new_path_exists;
    _impl->last_modif = new_last_modif;
    _impl->last = std::move(new_files);

    this->notify_observers(this);

    return true;
}

} // namespace sihd::util

#include <algorithm>
#include <vector>

#include <fmt/format.h>

#include <sihd/util/FileWatcher.hpp>
#include <sihd/util/container.hpp>
#include <sihd/util/fs.hpp>

namespace sihd::util
{

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

struct FileWatcher::FileWatch
{
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

FileWatcher::FileWatcher(const std::string & name, Node *parent): Named(name, parent)
{
    this->set_service_nb_thread(1);

    _step_worker.set_runnable(this);
    _step_worker.set_frequency(0.5);
    _step_worker.set_callback_setup([this] {
        this->notify_service_thread_started();
        if (_watch_path.empty() == false)
        {
            _file_watch_ptr->path_exists = fs::exists(_watch_path);
            _file_watch_ptr->last_modif = fs::last_write(_watch_path);

            map_path_into(_file_watch_ptr->last, _watch_path, _max_depth);
        }
    });

    _file_watch_ptr = std::make_unique<FileWatch>();

    this->add_conf("path", &FileWatcher::set_path);
    this->add_conf("max_depth", &FileWatcher::set_max_depth);
    this->add_conf("polling_frequency", &FileWatcher::set_polling_frequency);
}

FileWatcher::~FileWatcher()
{
    if (this->is_running())
        this->stop();
}

bool FileWatcher::set_path(std::string_view path)
{
    _watch_path = path;
    return true;
}

bool FileWatcher::set_max_depth(size_t depth)
{
    _max_depth = depth;
    return true;
}

bool FileWatcher::set_polling_frequency(double hz)
{
    return _step_worker.set_frequency(hz);
}

bool FileWatcher::is_running() const
{
    return _step_worker.is_worker_running();
}

bool FileWatcher::on_start()
{
    return _step_worker.start_sync_worker(this->name());
}

bool FileWatcher::on_stop()
{
    return _step_worker.stop_worker();
}

const std::vector<std::string> & FileWatcher::created() const
{
    return _file_watch_ptr->created;
}

const std::vector<std::string> & FileWatcher::removed() const
{
    return _file_watch_ptr->removed;
}

const std::vector<std::string> & FileWatcher::changed() const
{
    return _file_watch_ptr->changed;
}

size_t FileWatcher::watch_size() const
{
    return _file_watch_ptr->last.size() + (size_t)_file_watch_ptr->path_exists;
}

bool FileWatcher::run()
{
    const bool new_path_exists = fs::exists(_watch_path);
    const time_t new_last_modif = fs::last_write(_watch_path);

    std::vector<FileStat> new_files;
    new_files.reserve(_file_watch_ptr->last.size());
    map_path_into(new_files, _watch_path, _max_depth);

    _file_watch_ptr->created.clear();
    _file_watch_ptr->removed.clear();
    _file_watch_ptr->changed.clear();

    if (fs::is_dir(_watch_path))
    {
        put_intersections_into(_file_watch_ptr->__same_files, _file_watch_ptr->last, new_files);

        // build changed files list
        auto old_files_it = _file_watch_ptr->last.begin();
        const auto old_files_end = _file_watch_ptr->last.end();
        auto same_files_it = _file_watch_ptr->__same_files.begin();
        const auto same_files_end = _file_watch_ptr->__same_files.end();
        while (same_files_it != same_files_end)
        {
            while (old_files_it->path != same_files_it->path && old_files_it != old_files_end)
                ++old_files_it;

            if (old_files_it == old_files_end)
                break;

            if (old_files_it->last_accessed != fs::last_write(same_files_it->path))
                _file_watch_ptr->changed.emplace_back(old_files_it->path);

            ++same_files_it;
        }

        // build created files list
        put_differences_into(_file_watch_ptr->__created_files, new_files, _file_watch_ptr->__same_files);
        for (auto & file : _file_watch_ptr->__created_files)
        {
            _file_watch_ptr->created.emplace_back(std::move(file.path));
        }

        // build removed files list
        put_differences_into(_file_watch_ptr->__removed_files, _file_watch_ptr->last, new_files);
        for (auto & file : _file_watch_ptr->__removed_files)
        {
            _file_watch_ptr->removed.emplace_back(std::move(file.path));
        }
    }

    // create changes on the exact watch dir or file
    if (!new_path_exists && _file_watch_ptr->path_exists)
        _file_watch_ptr->removed.emplace_back(_watch_path);
    else if (new_path_exists && !_file_watch_ptr->path_exists)
        _file_watch_ptr->created.emplace_back(_watch_path);
    else if (_file_watch_ptr->last_modif < new_last_modif)
        _file_watch_ptr->changed.emplace_back(_watch_path);

    _file_watch_ptr->path_exists = new_path_exists;
    _file_watch_ptr->last_modif = new_last_modif;
    _file_watch_ptr->last = std::move(new_files);

    this->notify_observers(this);

    return true;
}

} // namespace sihd::util

#include <sihd/util/FS.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/platform.hpp>
#include <sihd/util/OS.hpp>
#include <sihd/util/File.hpp>
#include <sihd/util/Splitter.hpp>

#include <string.h> // strcmp
#include <dirent.h> // DIR...
#include <stdio.h> // remove

# if defined(__SIHD_WINDOWS__)
#  include <direct.h> // _mkdir _stat
# else
#  include <unistd.h>
# endif

namespace sihd::util
{

SIHD_LOGGER;

# if defined(__SIHD_WINDOWS__)
char FS::sep = '\\';
# else
char FS::sep = '/';
# endif

// stat

bool    FS::exists(std::string_view path)
{
    struct stat s;
    return OS::stat(path.data(), &s) == 0;
}

bool    FS::is_file(std::string_view path)
{
    struct stat s;
    if (OS::stat(path.data(), &s) == 0)
        return s.st_mode & S_IFREG;
    return false;
}

bool    FS::is_dir(std::string_view path)
{
    struct stat s;
    if (OS::stat(path.data(), &s) == 0)
        return s.st_mode & S_IFDIR;
    return false;
}

size_t    FS::filesize(std::string_view path)
{
    struct stat s;
    if (OS::stat(path.data(), &s) == 0)
        return s.st_size;
    return 0;
}

// directories

bool    FS::remove_directory(std::string_view path)
{
    return rmdir(path.data()) == 0;
}

bool    FS::remove_directories(std::string_view path)
{
    bool ret = true;
    std::vector<std::string> children = FS::recursive_children(path);
    for (auto it = children.rbegin(); it != children.rend(); ++it)
    {
        // remove maximum of entries
        if (FS::is_dir(*it))
        {
            if (FS::remove_directory(*it) == false)
            {
                SIHD_LOG(warning, "Files: cannot remove directory: " << *it);
                ret = false;
            }
        }
        else if (FS::remove_file(*it) == false)
        {
            SIHD_LOG(warning, "Files: cannot remove file: " << *it);
            ret = false;
        }
    }
    return ret;
}

bool    FS::make_directory(std::string_view path, mode_t mode)
{
    if (FS::is_dir(path))
        return true;
# if defined(__SIHD_WINDOWS__)
    (void)mode;
    return _mkdir(path.data()) == 0;
# else
    return mkdir(path.data(), mode) == 0;
# endif
}

bool    FS::make_directories(std::string_view path, mode_t mode)
{
    bool ret = true;
    if (!path.empty())
    {
        std::string sep(1, FS::sep);
        Splitter splitter(sep);
        std::vector<std::string> dirnames = splitter.split(path);
        std::string current_path = path[0] == FS::sep ? sep : "";
        for (const auto & dirname: dirnames)
        {
            current_path = FS::combine(current_path, dirname);
            if (FS::is_dir(current_path))
                continue ;
            ret = FS::make_directory(current_path, mode);
            if (ret == false)
                break ;
        }
    }
    return ret;
}

#if defined(__SIHD_WINDOWS__)

// TODO cannot link filesystem with mingw for the life of me

std::vector<std::string>    FS::children(std::string_view path)
{
    std::vector<std::string> ret;
    (void)path;
    /*
    for (const auto & dir_entry: filesystem::directory_iterator{path})
        ret.push_back(dir_entry.path().generic_string());
    */
    return ret;
}

std::vector<std::string>    FS::recursive_children(std::string_view path)
{
    std::vector<std::string> ret;
    (void)path;
    /*
    for (const auto & dir_entry: filesystem::recursive_directory_iterator{path})
        ret.push_back(dir_entry.path().generic_string());
    */
    return ret;
}

#else

void    FS::_get_recursive_children(std::string_view path, std::vector<std::string> & children)
{
    DIR *dir_ptr;
    struct dirent *dirent;
    if ((dir_ptr = opendir(path.data())) != NULL)
    {
        while ((dirent = readdir(dir_ptr)) != NULL)
        {
            if (strcmp(dirent->d_name, ".") == 0 || strcmp(dirent->d_name, "..") == 0)
                continue ;
            std::string childpath = FS::_combine(path, dirent->d_name);
            if (dirent->d_type & DT_DIR)
            {
                children.push_back(childpath + FS::sep);
                FS::_get_recursive_children(childpath, children);
            }
            else
            {
                children.push_back(childpath);
            }
        }
        closedir(dir_ptr);
    }
}

std::vector<std::string>    FS::recursive_children(std::string_view path)
{
    std::vector<std::string> ret;
    FS::_get_recursive_children(path, ret);
    return ret;
}

std::vector<std::string>    FS::children(std::string_view path)
{
    std::vector<std::string> ret;
    DIR *dir_ptr;
    struct dirent *dirent;
    if ((dir_ptr = opendir(path.data())) != NULL)
    {
        while ((dirent = readdir(dir_ptr)) != NULL)
        {
            if (strcmp(dirent->d_name, ".") == 0 || strcmp(dirent->d_name, "..") == 0)
                continue ;
            if (dirent->d_type & DT_DIR)
                ret.push_back(std::string(dirent->d_name) + FS::sep);
            else
                ret.push_back(dirent->d_name);
        }
    }
    closedir(dir_ptr);
    return ret;
}

#endif

// path manipulation

std::string FS::normalize(std::string_view path)
{
    std::string sep(1, FS::sep);
    Splitter splitter(sep);
    std::vector<std::string> lst;
    std::vector<std::string> splits = splitter.split(path);

    for (const std::string & split: splits)
    {
        if (split.find("..") == 0 && lst.size() > 0)
            lst.pop_back();
        else
            lst.push_back(split);
    }
    bool start_with_slash = path.size() > 0 && path[0] == FS::sep;
    std::string ret = Str::join(lst, sep);
    if (start_with_slash)
        ret.insert(0, 1, FS::sep);
    return ret;
}

void    FS::trim_in_path(std::string & path, std::string_view to_remove)
{
    size_t idx = path.find(to_remove);
    if (idx == std::string::npos)
        return ;
    idx = idx + to_remove.size();
    if (path[idx] == FS::sep)
        ++idx;
    path = path.substr(idx, path.size());
}

void    FS::trim_in_path(std::vector<std::string> & list, std::string_view to_remove)
{
    for (auto & path: list)
        FS::trim_in_path(path, to_remove);
}

std::string    FS::trim_path(std::string_view path, std::string_view to_remove)
{
    std::string ret(path.data(), path.size());
    FS::trim_in_path(ret, to_remove);
    return ret;
}

std::string     FS::extension(std::string_view path)
{
    std::string ret;
    size_t slash_idx = path.find_last_of(FS::sep);
    if (slash_idx == std::string::npos)
        slash_idx = 0;
    size_t first_dot_idx = path.find_first_of('.', slash_idx);
    if (first_dot_idx != std::string::npos)
        ret = path.substr(first_dot_idx + 1, path.size());
    return ret;
}

std::string     FS::filename(std::string_view path)
{
    size_t idx = path.find_last_of(FS::sep);
    if (idx == std::string::npos)
        return std::string(path.data(), path.size());
    std::string_view filename = path.substr(idx + 1);
    return std::string(filename.data(), filename.size());
}

std::string     FS::parent(std::string_view path)
{
    // removing extra slashes: /path/to/dir///// -> /path/to/dir
    size_t i = path.size() == 0 ? 0 : path.size() - 1;
    while (i > 0 && path[i] == FS::sep)
        --i;
    // removing trailing slashes: /path/to/////dir -> /path/to
    std::string ret;
    size_t idx = path.find_last_of(FS::sep, i);
    while (idx != std::string::npos && idx > 0)
    {
        if (path[idx - 1] != FS::sep)
        {
            ret = path.substr(0, idx);
            break ;
        }
        --idx;
    }
    return ret;
}

std::string  FS::combine(std::initializer_list<std::string_view> list)
{
    std::string ret;

    for (const std::string_view & path: list)
    {
        ret = FS::combine(ret, path);
    }
    return ret;
}

std::string  FS::combine(const std::vector<std::string> & list)
{
    std::string ret;

    for (const std::string & path: list)
    {
        ret = FS::combine(ret, path);
    }
    return ret;
}

std::string FS::combine(std::string_view path1, std::string_view path2)
{
    return FS::_combine(path1, path2);
}

std::string FS::_combine(std::string_view path1, std::string_view path2)
{
    if (path1.empty())
        return std::string(path2.data(), path2.size());
    if (path1[path1.size() - 1] == FS::sep)
        return std::string(path1.data(), path1.size()) + path2.data();
    std::string ret;
    ret.reserve(path1.size() + 1 + path2.size());
    ret.append(path1);
    ret.push_back(FS::sep);
    ret.append(path2);
    return ret;
}

bool    FS::is_absolute(std::string_view path)
{
#if defined(__SIHD_WINDOWS__)
    return (path.length() > 1 && path[0] == FS::sep && path[1] == FS::sep)
            || (path.length() > 2 && path[1] == ':' && path[2] == FS::sep);
#else
    return path.length() > 0 && path[0] == FS::sep;
#endif
}

// files

bool    FS::are_equals(std::string_view path1, std::string_view path2)
{
    File file1(path1, "rb");
    File file2(path2, "rb");

    if (!file1.is_open() || !file2.is_open())
        return false;
    ssize_t read_count;
    size_t buffer_size = 4096;
    char buffer1[buffer_size];
    char buffer2[buffer_size];
    while ((read_count = file1.read(buffer1, buffer_size)) > 0)
    {
        if (file2.read(buffer2, buffer_size) != read_count)
            return false;
        if (::memcmp(buffer1, buffer2, read_count) != 0)
            return false;
    }
    return true;
}

bool    FS::remove_file(std::string_view path)
{
    return remove(path.data()) == 0;
}

bool    FS::write(std::string_view path, ArrViewChar view, bool append)
{
    File file(path, append ? "a" : "w");

    if (file.is_open())
        return file.write(view) == (ssize_t)view.byte_size();
    return false;
}

bool    FS::write_binary(std::string_view path, ArrViewChar view, bool append)
{
    File file(path, append ? "ab" : "wb");

    if (file.is_open())
        return file.write(view) == (ssize_t)view.byte_size();
    return -1;
}

std::optional<std::string>  FS::read(std::string_view path, size_t size)
{
    File file(path, "r");

    if (file.is_open())
    {
        char buf[size + 1];
        ssize_t ret;
        if ((ret = file.read(buf, size)) > 0)
        {
            buf[ret] = 0;
            return std::string(buf, ret);
        }
    }
    return std::nullopt;
}

std::optional<std::string>  FS::read_all(std::string_view path)
{
    std::ifstream file(path.data(), std::ifstream::in);
    if (file.is_open() && file.good())
    {
        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();
        return buffer.str();
    }
    return std::nullopt;
}

ssize_t FS::read_binary(std::string_view path, char *buf, size_t size)
{
    File file(path, "rb");

    ssize_t ret = -1;
    if (file.is_open())
        return file.read(buf, size);
    return ret;
}

}
#include <sihd/util/Files.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/platform.hpp>

#include <sys/types.h>
#include <sys/stat.h> // stat
#include <string.h> // strcmp
#include <dirent.h> // DIR...
#include <stdio.h> // remove

# if defined(__SIHD_WINDOWS__)
#  include <direct.h> // _mkdir _stat
#  define stat _stat
#  define mkdir _mkdir
#  define rmdir _rmdir
# else
#  include <unistd.h>
# endif

namespace sihd::util
{

LOGGER;

# if defined(__SIHD_WINDOWS__)
char Files::sep = '\\';
# else
char Files::sep = '/';
# endif

// stat

bool    Files::exists(const std::string & path)
{
    struct stat s;
    return stat(path.c_str(), &s) == 0;
}

bool    Files::is_file(const std::string & path)
{
    struct stat s;
    if (stat(path.c_str(), &s) == 0)
        return s.st_mode & S_IFREG;
    return false;
}

bool    Files::is_dir(const std::string & path)
{
    struct stat s;
    if (stat(path.c_str(), &s) == 0)
        return s.st_mode & S_IFDIR;
    return false;
}

size_t    Files::get_filesize(const std::string & path)
{
    struct stat s;
    if (stat(path.c_str(), &s) == 0)
        return s.st_size;
    return 0;
}

// directories

bool    Files::remove_directory(const std::string & path)
{
    return rmdir(path.c_str()) == 0;
}

bool    Files::remove_directories(const std::string & path)
{
    bool ret = true;
    std::vector<std::string> children = Files::get_recursive_children(path);
    for (auto it = children.rbegin(); it != children.rend(); ++it)
    {
        // remove maximum of entries
        if (Files::is_dir(*it))
        {
            if (Files::remove_directory(*it) == false)
            {
                LOG(warning, "Files: cannot remove directory: " << *it);
                ret = false;
            }
        }
        else if (Files::remove_file(*it) == false)
        {
            LOG(warning, "Files: cannot remove file: " << *it);
            ret = false;
        }
    }
    return ret;
}

bool    Files::make_directory(const std::string & path, mode_t mode)
{
    if (Files::is_dir(path))
        return true;
# if defined(__SIHD_WINDOWS__)
    (void)mode;
    return _mkdir(path.c_str()) == 0;
# else
    return mkdir(path.c_str(), mode) == 0;
# endif
}

bool    Files::make_directories(const std::string & path, mode_t mode)
{
    bool ret = true;
    if (!path.empty())
    {
        std::string sep(1, Files::sep);
        std::vector<std::string> dirnames = Str::split(path, sep.c_str());
        std::string current_path = path[0] == Files::sep ? sep : "";
        for (const auto & dirname: dirnames)
        {
            current_path = Files::combine(current_path, dirname);
            if (Files::is_dir(current_path))
                continue ;
            ret = Files::make_directory(current_path, mode);
            if (ret == false)
                break ;
        }
    }
    return ret;
}

#if defined(__SIHD_WINDOWS__)

// TODO cannot link filesystem with mingw for the life of me

std::vector<std::string>    Files::get_children(const std::string & path)
{
    std::vector<std::string> ret;
    (void)path;
    /*
    for (const auto & dir_entry: filesystem::directory_iterator{path})
        ret.push_back(dir_entry.path().generic_string());
    */
    return ret;
}

std::vector<std::string>    Files::get_recursive_children(const std::string & path)
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

void    Files::_get_recursive_children(const std::string & path, std::vector<std::string> & children)
{
    DIR *dir_ptr;
    struct dirent *dirent;
    if ((dir_ptr = opendir(path.c_str())) != NULL)
    {
        while ((dirent = readdir(dir_ptr)) != NULL)
        {
            if (strcmp(dirent->d_name, ".") == 0 || strcmp(dirent->d_name, "..") == 0)
                continue ;
            std::string childpath = Files::combine(path, dirent->d_name);
            if (dirent->d_type & DT_DIR)
            {
                children.push_back(childpath + Files::sep);
                Files::_get_recursive_children(childpath, children);
            }
            else
                children.push_back(childpath);
        }
    }
    closedir(dir_ptr);
}

std::vector<std::string>    Files::get_recursive_children(const std::string & path)
{
    std::vector<std::string> ret;
    Files::_get_recursive_children(path, ret);
    return ret;
}

std::vector<std::string>    Files::get_children(const std::string & path)
{
    std::vector<std::string> ret;
    DIR *dir_ptr;
    struct dirent *dirent;
    if ((dir_ptr = opendir(path.c_str())) != NULL)
    {
        while ((dirent = readdir(dir_ptr)) != NULL)
        {
            if (strcmp(dirent->d_name, ".") == 0 || strcmp(dirent->d_name, "..") == 0)
                continue ;
            if (dirent->d_type & DT_DIR)
                ret.push_back(std::string(dirent->d_name) + Files::sep);
            else
                ret.push_back(dirent->d_name);
        }
    }
    closedir(dir_ptr);
    return ret;
}

#endif

// path manipulation

std::string Files::normalize(const std::string & path)
{
    std::string sep(1, Files::sep);
    std::vector<std::string> lst;
    std::vector<std::string> splits = Str::split(path, sep.c_str());

    for (const std::string & split: splits)
    {
        if (split.find("..") == 0 && lst.size() > 0)
            lst.pop_back();
        else
            lst.push_back(split);
    }
    bool start_with_slash = path.size() > 0 && path[0] == Files::sep;
    std::string ret = Str::join(lst, sep);
    if (start_with_slash)
        ret.insert(0, 1, Files::sep);
    return ret;
}

void    Files::trim_in_path(std::string & path, const std::string & to_remove)
{
    size_t idx = path.find(to_remove);
    if (idx == std::string::npos)
        return ;
    idx = idx + to_remove.size();
    if (path[idx] == Files::sep)
        ++idx;
    path = path.substr(idx, path.size());
}

void    Files::trim_in_path(std::vector<std::string> & list, const std::string & to_remove)
{
    for (auto & path: list)
        Files::trim_in_path(path, to_remove);
}

std::string    Files::trim_path(std::string && path, const std::string & to_remove)
{
    Files::trim_in_path(path, to_remove);
    return std::move(path);
}

std::string    Files::trim_path(const std::string & path, const std::string & to_remove)
{
    std::string ret = path;
    Files::trim_in_path(ret, to_remove);
    return ret;
}

std::string     Files::get_extension(const std::string & path)
{
    std::string ret;
    size_t slash_idx = path.find_last_of(Files::sep);
    if (slash_idx == std::string::npos)
        slash_idx = 0;
    size_t first_dot_idx = path.find_first_of('.', slash_idx);
    if (first_dot_idx != std::string::npos)
        ret = path.substr(first_dot_idx + 1, path.size());
    return ret;
}

std::string     Files::get_filename(const std::string & path)
{
    size_t idx = path.find_last_of(Files::sep);
    if (idx == std::string::npos)
        return path;
    std::string ret = path.substr(idx + 1, path.size());
    return ret;
}

std::string     Files::get_parent(const std::string & path)
{
    // removing extra slashes: /path/to/dir///// -> /path/to/dir
    size_t i = path.size() == 0 ? 0 : path.size() - 1;
    while (i > 0 && path[i] == Files::sep)
        --i;
    // removing trailing slashes: /path/to/////dir -> /path/to
    std::string ret;
    size_t idx = path.find_last_of(Files::sep, i);
    while (idx != std::string::npos && idx > 0)
    {
        if (path[idx - 1] != Files::sep)
        {
            ret = path.substr(0, idx);
            break ;
        }
        --idx;
    }
    return ret;
}

std::string  Files::combine(std::vector<std::string> && list)
{
    std::string ret;

    for (const std::string & path: list)
    {
        ret = Files::combine(ret, path);
    }
    return ret;
}

std::string  Files::combine(const std::vector<std::string> & list)
{
    std::string ret;

    for (const std::string & path: list)
    {
        ret = Files::combine(ret, path);
    }
    return ret;
}

std::string Files::combine(const std::string & path1, const std::string & path2)
{
    if (path1.empty())
        return path2;
    if (path1[path1.size() - 1] == Files::sep)
        return path1 + path2;
    return path1 + Files::sep + path2;
}

// files

bool    Files::remove_file(const std::string & path)
{
    return remove(path.c_str()) == 0;
}

bool    Files::write_all(const std::string & path, const std::string & content, bool append)
{
    std::ofstream os;

    if (append)
        os.open(path, std::ofstream::out | std::fstream::app);
    else
        os.open(path, std::ofstream::out);
    if (os.is_open())
    {
        os << content;
        os.close();
        return os.good();
    }
    return false;
}

std::optional<std::string>     Files::read_all(const std::string & path)
{
    std::ifstream is(path, std::ifstream::in);
    if (is.is_open())
    {
        std::stringstream buffer;
        buffer << is.rdbuf();
        is.close();
        return buffer.str();
    }
    return {};
}

}
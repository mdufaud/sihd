#include <dirent.h> // DIR...
#include <sys/stat.h>

#include <cstdio>  // remove
#include <cstring> // strcmp
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include <sihd/util/File.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Splitter.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/platform.hpp>

#if defined(__SIHD_WINDOWS__)
# include <direct.h> // _mkdir _stat
# include <fileapi.h>
# include <libloaderapi.h>
#else
# include <unistd.h>
#endif

namespace sihd::util::fs
{

SIHD_NEW_LOGGER("sihd::util::fs");

namespace
{

#if defined(__SIHD_WINDOWS__)
char g_separator_char = '\\';
#else
char g_separator_char = '/';
#endif

bool _is_file_type(std::string_view path, std::filesystem::file_type expected_type)
{
    std::error_code ec;
    auto status = std::filesystem::status(path, ec);
    if (ec)
    {
        SIHD_LOG(error, "status: {}: {}", ec.message(), path);
        return false;
    }
    return status.type() == expected_type;
}

std::string _combine(std::string_view path1, std::string_view path2)
{
    if (path1.empty())
        return std::string(path2.data(), path2.size());
    if (path1[path1.size() - 1] == g_separator_char)
        return std::string(path1.data(), path1.size()) + path2.data();
    std::string ret;
    ret.reserve(path1.size() + 1 + path2.size());
    ret.append(path1);
    ret.push_back(g_separator_char);
    ret.append(path2);
    return ret;
}

#if !defined(__SIHD_WINDOWS__)

void _get_recursive_children(std::string_view path,
                             std::vector<std::string> & children,
                             uint32_t current_depth,
                             uint32_t max_depth)
{
    if (max_depth > 0 && current_depth >= max_depth)
        return;

    DIR *dir_ptr;
    struct dirent *dirent;
    if ((dir_ptr = opendir(path.data())) != NULL)
    {
        while ((dirent = readdir(dir_ptr)) != NULL)
        {
            if (strcmp(dirent->d_name, ".") == 0 || strcmp(dirent->d_name, "..") == 0)
                continue;
            std::string childpath = _combine(path, dirent->d_name);
            if (dirent->d_type & DT_DIR)
            {
                children.push_back(childpath + g_separator_char);
                _get_recursive_children(childpath, children, current_depth + 1, max_depth);
            }
            else
            {
                children.push_back(childpath);
            }
        }
        closedir(dir_ptr);
    }
}

#endif

bool internal_set_perm(std::string_view path, unsigned int mode, std::filesystem::perm_options option)
{
    std::error_code ec;
    std::filesystem::permissions(path, static_cast<std::filesystem::perms>(mode), option, ec);
    if (ec)
        SIHD_LOG(error, "permissions: {}: {}", ec.message(), path);
    return !ec;
}

bool do_stat(std::string_view path, struct stat *s)
{
#if defined(__SIHD_WINDOWS__)
    return ::_stat(path.data(), reinterpret_cast<struct _stat *>(s)) == 0;
#else
    return ::stat(path.data(), s) == 0;
#endif
}

} // namespace

// utils

void set_sep(char c)
{
    g_separator_char = c;
}

char sep()
{
    return g_separator_char;
}

std::string sep_str()
{
    return std::string(1, g_separator_char);
};

std::string home_path()
{
#if defined(__SIHD_WINDOWS__)
    return combine(getenv("HOMEDRIVE"), getenv("HOMEPATH"));
#else
    return getenv("HOME");
#endif
}

std::string cwd()
{
    char cwd[PATH_MAX];

    if (getcwd(cwd, sizeof(cwd)) != nullptr)
        return std::string(cwd);
    return "";
}

std::string executable_path()
{
#if defined(__SIHD_EMSCRIPTEN__)
    return "";
#elif defined(__SIHD_WINDOWS__)
    char path[MAX_PATH];
    if (GetModuleFileName(NULL, path, MAX_PATH) != 0)
        return path;
#else
    std::string path;
    try
    {
        path = std::filesystem::canonical("/proc/self/exe");
        if (path.empty() == false)
            return path;
    }
    catch ([[maybe_unused]] const std::filesystem::filesystem_error & e)
    {
    }
    std::ifstream mapf("/proc/self/maps");
    std::string line;
    if (std::getline(mapf, line))
    {
        size_t idx = line.find("/");
        if (idx != std::string::npos)
        {
            path = line.substr(idx);
            return path;
        }
    }
#endif
    return ".";
}

// stat

bool exists(std::string_view path)
{
#if defined(__SIHD_WINDOWS__)
    return _access(path.data(), 0) == 0;
#else
    return access(path.data(), F_OK) == 0;
#endif
}

bool is_readable(std::string_view path)
{
#if defined(__SIHD_WINDOWS__)
    return _access(path.data(), 04) == 0;
#else
    return access(path.data(), R_OK) == 0;
#endif
}

bool is_writable(std::string_view path)
{
#if defined(__SIHD_WINDOWS__)
    return _access(path.data(), 02) == 0;
#else
    return access(path.data(), W_OK) == 0;
#endif
}

bool is_executable(std::string_view path)
{
#if defined(__SIHD_WINDOWS__)
    return _access(path.data(), 04) == 0;
#else
    return access(path.data(), X_OK) == 0;
#endif
}

bool is_file(std::string_view path)
{
    return _is_file_type(path, std::filesystem::file_type::regular);
}

bool is_dir(std::string_view path)
{
    return _is_file_type(path, std::filesystem::file_type::directory);
}

bool is_symlink(std::string_view path)
{
    return _is_file_type(path, std::filesystem::file_type::symlink);
}

bool is_socket(std::string_view path)
{
    return _is_file_type(path, std::filesystem::file_type::socket);
}

bool is_block(std::string_view path)
{
    return _is_file_type(path, std::filesystem::file_type::block);
}

bool is_character(std::string_view path)
{
    return _is_file_type(path, std::filesystem::file_type::character);
}

bool is_fifo(std::string_view path)
{
    return _is_file_type(path, std::filesystem::file_type::fifo);
}

Timestamp last_write(std::string_view path)
{
    struct stat s;
    return do_stat(path.data(), &s) ? Timestamp(time::seconds(s.st_mtime)) : Timestamp {};
}

std::optional<size_t> file_size(std::string_view path)
{
    struct stat s;
    return do_stat(path.data(), &s) ? s.st_size : std::optional<size_t> {};
}

std::string permission_to_str(unsigned int mode)
{
    using std::filesystem::perms;
    perms p = static_cast<perms>(mode);
    std::string ret;
    ret.reserve(9); // in case there is no short string opt ?
    ret += (p & perms::owner_read) == perms::none ? "-" : "r";
    ret += (p & perms::owner_write) == perms::none ? "-" : "w";
    ret += (p & perms::owner_exec) == perms::none ? "-" : "x";
    ret += (p & perms::group_read) == perms::none ? "-" : "r";
    ret += (p & perms::group_write) == perms::none ? "-" : "w";
    ret += (p & perms::group_exec) == perms::none ? "-" : "x";
    ret += (p & perms::others_read) == perms::none ? "-" : "r";
    ret += (p & perms::others_write) == perms::none ? "-" : "w";
    ret += (p & perms::others_exec) == perms::none ? "-" : "x";
    return ret;
}

unsigned int permission_from_str(std::string_view mode)
{
    using std::filesystem::perms;
    const size_t mode_size = mode.size();
    perms p = perms::none;
    if (mode_size > 0 && mode[0] == 'r')
        p |= perms::owner_read;
    if (mode_size > 1 && mode[1] == 'w')
        p |= perms::owner_write;
    if (mode_size > 2 && mode[2] == 'x')
        p |= perms::owner_exec;
    if (mode_size > 3 && mode[3] == 'r')
        p |= perms::group_read;
    if (mode_size > 4 && mode[4] == 'w')
        p |= perms::group_write;
    if (mode_size > 5 && mode[5] == 'x')
        p |= perms::group_exec;
    if (mode_size > 6 && mode[6] == 'r')
        p |= perms::others_read;
    if (mode_size > 7 && mode[7] == 'w')
        p |= perms::others_write;
    if (mode_size > 8 && mode[8] == 'x')
        p |= perms::others_exec;
    return static_cast<unsigned int>(p);
}

bool permission_add(std::string_view path, unsigned int mode)
{
    return internal_set_perm(path, mode, std::filesystem::perm_options::add);
}

bool permission_rm(std::string_view path, unsigned int mode)
{
    return internal_set_perm(path, mode, std::filesystem::perm_options::remove);
}

bool permission_set(std::string_view path, unsigned int mode)
{
    return internal_set_perm(path, mode, std::filesystem::perm_options::replace);
}

unsigned int permission_get(std::string_view path)
{
    std::error_code ec;
    auto p = std::filesystem::status(path, ec).permissions();
    if (ec)
        SIHD_LOG(error, "permission_get: {}: {}", ec.message(), path);
    return ec ? 0 : static_cast<unsigned int>(p);
}

// directories

std::string tmp_path()
{
#if defined(__SIHD_WINDOWS__)
    try
    {
        return std::filesystem::temp_directory_path().string();
    }
    catch ([[maybe_unused]] const std::filesystem::filesystem_error & e)
    {
    }
    const char *tmp_path = getenv("Temp");
    return tmp_path != nullptr ? tmp_path : "C:\\Windows\\TEMP\\";
#else
    const char *tmp_path;

    (tmp_path = getenv("TMPDIR")) || (tmp_path = getenv("TMP")) || (tmp_path = getenv("TEMP"))
        || (tmp_path = getenv("TMPDIR"));
    return tmp_path != nullptr ? tmp_path : "/tmp";
#endif
}

std::string make_tmp_directory(std::string_view prefix)
{
#if defined(__SIHD_WINDOWS__)
    (void)prefix;
    std::string path = std::tmpnam(nullptr);
    if (make_directory(path))
        return path;
#else
    if (prefix.size() + 6 > PATH_MAX)
        throw std::runtime_error(fmt::format("make_tmp_directory: path too long: {}", prefix));

    std::string path;
    path.reserve(prefix.size() + 6 + 1);
    path += prefix;
    path += "XXXXXX";
    if (mkdtemp(path.data()) != nullptr)
        return path;
#endif
    return "";
}

bool remove_directory(std::string_view path)
{
    return rmdir(path.data()) == 0;
}

bool remove_directories(std::string_view path)
{
    bool ret = true;
    std::vector<std::string> children = recursive_children(path);
    for (auto it = children.rbegin(); it != children.rend(); ++it)
    {
        // remove maximum of entries
        if (is_dir(*it))
        {
            if (remove_directory(*it) == false)
            {
                SIHD_LOG(warning, "cannot remove directory: {}", *it);
                ret = false;
            }
        }
        else if (remove_file(*it) == false)
        {
            SIHD_LOG(warning, "cannot remove file: {}", *it);
            ret = false;
        }
    }
    return ret;
}

bool make_directory(std::string_view path, unsigned int mode)
{
    if (is_dir(path))
        return true;
    if (path.empty())
        return false;
#if defined(__SIHD_WINDOWS__)
    (void)mode;
    return _mkdir(path.data()) == 0;
#else
    return mkdir(path.data(), mode) == 0;
#endif
}

bool make_directories(std::string_view path, unsigned int mode)
{
    bool ret = true;
    if (!path.empty())
    {
        std::string separator = sep_str();
        Splitter splitter(separator);
        std::vector<std::string> dirnames = splitter.split(path);
        std::string current_path = path[0] == g_separator_char ? separator : "";
        for (const auto & dirname : dirnames)
        {
            current_path = combine(current_path, dirname);
            if (is_dir(current_path))
                continue;
            ret = make_directory(current_path, mode);
            if (ret == false)
                break;
        }
    }
    return ret;
}

#if defined(__SIHD_WINDOWS__)

std::vector<std::string> children(std::string_view path)
{
    std::vector<std::string> ret;

    std::error_code ec;
    const auto options = std::filesystem::directory_options::skip_permission_denied;
    std::filesystem::directory_iterator it {path, options, ec};
    std::filesystem::directory_iterator end;

    while (it != end)
    {
        ret.push_back(it->path().string());
        it = it.increment(ec);
    }

    return ret;
}

std::vector<std::string> recursive_children(std::string_view path, uint32_t max_depth)
{
    std::vector<std::string> ret;

    std::error_code ec;
    const auto options = std::filesystem::directory_options::skip_permission_denied;
    std::filesystem::recursive_directory_iterator it {path, options, ec};
    std::filesystem::recursive_directory_iterator end;

    while (it != end)
    {
        if (max_depth == 0 || (uint32_t)it.depth() < max_depth)
        {
            ret.push_back(it->path().string());
        }
        it = it.increment(ec);
    }

    return ret;
}

#else

std::vector<std::string> recursive_children(std::string_view path, uint32_t max_depth)
{
    std::vector<std::string> ret;
    uint32_t current_depth = 0;
    _get_recursive_children(path, ret, current_depth, max_depth);
    return ret;
}

std::vector<std::string> children(std::string_view path)
{
    std::vector<std::string> ret;
    DIR *dir_ptr;
    struct dirent *dirent;
    if ((dir_ptr = opendir(path.data())) != NULL)
    {
        while ((dirent = readdir(dir_ptr)) != NULL)
        {
            if (strcmp(dirent->d_name, ".") == 0 || strcmp(dirent->d_name, "..") == 0)
                continue;
            if (dirent->d_type & DT_DIR)
                ret.push_back(std::string(dirent->d_name) + g_separator_char);
            else
                ret.push_back(dirent->d_name);
        }
    }
    closedir(dir_ptr);
    return ret;
}

#endif

// path manipulation

std::string normalize(std::string_view path)
{
    std::string separator = sep_str();
    Splitter splitter(separator);
    std::vector<std::string> lst;
    std::vector<std::string> splits = splitter.split(path);

    for (const std::string & split : splits)
    {
        if (split.find("..") == 0 && lst.size() > 0)
            lst.pop_back();
        else
            lst.push_back(split);
    }
    bool start_with_slash = path.size() > 0 && path[0] == g_separator_char;
    std::string ret = fmt::format("{}", fmt::join(lst, separator));
    if (start_with_slash)
        ret.insert(0, 1, g_separator_char);
    return ret;
}

void trim_in_path(std::string & path, std::string_view to_remove)
{
    size_t idx = path.find(to_remove);
    if (idx == std::string::npos)
        return;
    idx = idx + to_remove.size();
    if (path[idx] == g_separator_char)
        ++idx;
    path = path.substr(idx, path.size());
}

void trim_in_path(std::vector<std::string> & list, std::string_view to_remove)
{
    for (auto & path : list)
        trim_in_path(path, to_remove);
}

std::string trim_path(std::string_view path, std::string_view to_remove)
{
    std::string ret(path.data(), path.size());
    trim_in_path(ret, to_remove);
    return ret;
}

std::string extension(std::string_view path)
{
    std::string ret;
    size_t slash_idx = path.find_last_of(g_separator_char);
    if (slash_idx == std::string::npos)
        slash_idx = 0;
    size_t first_dot_idx = path.find_first_of('.', slash_idx);
    if (first_dot_idx != std::string::npos)
        ret = path.substr(first_dot_idx + 1, path.size());
    return ret;
}

std::string filename(std::string_view path)
{
    size_t idx = path.find_last_of(g_separator_char);
    if (idx == std::string::npos)
        return std::string(path.data(), path.size());
    std::string_view filename = path.substr(idx + 1);
    return std::string(filename.data(), filename.size());
}

std::string parent(std::string_view path)
{
    // removing extra slashes: /path/to/dir///// -> /path/to/dir
    size_t i = path.size() == 0 ? 0 : path.size() - 1;
    while (i > 0 && path[i] == g_separator_char)
        --i;
    // removing trailing slashes: /path/to/////dir -> /path/to
    std::string ret;
    size_t idx = path.find_last_of(g_separator_char, i);
    while (idx != std::string::npos && idx > 0)
    {
        if (path[idx - 1] != g_separator_char)
        {
            ret = path.substr(0, idx);
            break;
        }
        --idx;
    }
    return ret;
}

std::string combine(std::initializer_list<std::string_view> list)
{
    std::string ret;

    for (const std::string_view & path : list)
    {
        ret = combine(ret, path);
    }
    return ret;
}

std::string combine(const std::vector<std::string> & list)
{
    std::string ret;

    for (const std::string & path : list)
    {
        ret = combine(ret, path);
    }
    return ret;
}

std::string combine(std::string_view path1, std::string_view path2)
{
    return _combine(path1, path2);
}

std::string ensure_separation(std::string_view path)
{
    if (path.empty())
        return "";
    if (path.at(path.size() - 1) == g_separator_char)
        return std::string(path);
    std::string ret;
    ret.reserve(path.size() + 1);
    ret.append(path);
    ret += g_separator_char;
    return ret;
}

bool is_absolute(std::string_view path)
{
#if defined(__SIHD_WINDOWS__)
    return (path.length() > 1 && path[0] == g_separator_char && path[1] == g_separator_char)
           || (path.length() > 2 && path[1] == ':' && path[2] == g_separator_char);
#else
    return path.length() > 0 && path[0] == g_separator_char;
#endif
}

// files

std::optional<std::string> read_link(std::string_view path)
{
    std::error_code ec;
    std::filesystem::path link_path = std::filesystem::read_symlink(path, ec);
    if (ec)
    {
        SIHD_LOG(error, "read_link: {}: {}", ec.message(), path);
        return std::nullopt;
    }
    return {link_path.string()};
}

bool make_file_link(std::string_view target, std::string_view link)
{
    std::error_code ec;
    std::filesystem::create_symlink(target, link, ec);
    if (ec)
        SIHD_LOG(error, "make_file_link: {}: {} -> {}", ec.message(), target, link);
    return ec.value() == 0;
}

bool make_dir_link(std::string_view target, std::string_view link)
{
    std::error_code ec;
    std::filesystem::create_directory_symlink(target, link, ec);
    if (ec)
        SIHD_LOG(error, "make_dir_link: {}: {} -> {}", ec.message(), target, link);
    return ec.value() == 0;
}

bool make_hard_link(std::string_view target, std::string_view link)
{
    std::error_code ec;
    std::filesystem::create_hard_link(target, link, ec);
    if (ec)
        SIHD_LOG(error, "make_hard_link: {}: {} -> {}", ec.message(), target, link);
    return ec.value() == 0;
}

bool are_equals(std::string_view path1, std::string_view path2)
{
    File file1(path1, "rb");
    File file2(path2, "rb");

    if (!file1.is_open() || !file2.is_open())
        return false;

    if (file1.file_size() != file2.file_size())
        return false;

    ssize_t read_count;
    constexpr size_t buffer_size = 4096;
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

bool remove_file(std::string_view path)
{
    return remove(path.data()) == 0;
}

bool write(std::string_view path, std::string_view view, bool append)
{
    File file(path, append ? "a" : "w");

    if (file.is_open())
        return file.write(view) == (ssize_t)view.size();
    return false;
}

bool write_binary(std::string_view path, std::string_view view, bool append)
{
    File file(path, append ? "ab" : "wb");

    if (file.is_open())
        return file.write(view) == (ssize_t)view.size();
    return -1;
}

std::optional<std::string> read(std::string_view path, size_t size, long offset)
{
    File file(path, "r");

    if (!file.is_open())
        return std::nullopt;

    if (offset > 0)
        file.seek_begin(offset);
    else if (offset < 0)
        file.seek_end(-offset);

    ssize_t ret;
    std::string str;
    if ((ret = file.read(str, size)) > 0)
    {
        str[ret] = 0;
        return str;
    }

    return std::nullopt;
}

std::optional<std::string> read_all(std::string_view path)
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

ssize_t read_binary(std::string_view path, char *buf, size_t size)
{
    File file(path, "rb");

    ssize_t ret = -1;
    if (file.is_open())
        return file.read(buf, size);
    return ret;
}

bool chdir(std::string_view path)
{
#if defined(__SIHD_WINDOWS__)
    return ::_chdir(path.data()) == 0;
#else
    return ::chdir(path.data()) == 0;
#endif
}

} // namespace sihd::util::fs

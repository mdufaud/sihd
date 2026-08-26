#include <unistd.h> // getcwd

#include <cstdio> // remove
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <fmt/ranges.h>

#include <sihd/sys/File.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Splitter.hpp>
#include <sihd/util/build.hpp>

// home/executable paths, children iteration, truncation, mount and storage queries,
// chdir and tmp directories live in src/linux|windows/fs.cpp

using namespace sihd::util;
namespace sihd::sys::fs
{

SIHD_NEW_LOGGER("sihd::sys::fs");

namespace
{

char g_separator_char = sihd::util::build::is_windows ? '\\' : '/';

bool is_file_type(std::string_view path, std::filesystem::file_type expected_type)
{
    std::error_code ec;
    auto status = std::filesystem::status(path, ec);
    if (ec)
        return false;
    return status.type() == expected_type;
}

std::string combine_impl(std::string_view path1, std::string_view path2)
{
    if (path1.empty())
        return std::string(path2.data(), path2.size());
    if (path1[path1.size() - 1] == g_separator_char)
        return fmt::format("{0}{1}", path1, path2);
    return fmt::format("{0}{1}{2}", path1, g_separator_char, path2);
}

bool internal_set_perm(std::string_view path, unsigned int mode, std::filesystem::perm_options option)
{
    std::error_code ec;
    std::filesystem::permissions(path, static_cast<std::filesystem::perms>(mode), option, ec);
    if (ec)
        SIHD_LOG(debug, "permissions: {}: {}", ec.message(), path);
    return !ec;
}

template <typename T>
std::string combine_lst_impl(const T & list)
{
    std::string ret;

    for (const auto & path : list)
    {
        ret = combine(ret, path);
    }

    return ret;
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

std::string cwd()
{
    char cwd[PATH_MAX];

    if (getcwd(cwd, sizeof(cwd)) != nullptr)
        return std::string(cwd);
    return "";
}

// stat

bool is_file(std::string_view path)
{
    return is_file_type(path, std::filesystem::file_type::regular);
}

bool is_dir(std::string_view path)
{
    return is_file_type(path, std::filesystem::file_type::directory);
}

bool is_symlink(std::string_view path)
{
    std::error_code ec;
    return std::filesystem::symlink_status(path, ec).type() == std::filesystem::file_type::symlink;
}

bool is_socket(std::string_view path)
{
    return is_file_type(path, std::filesystem::file_type::socket);
}

bool is_block(std::string_view path)
{
    return is_file_type(path, std::filesystem::file_type::block);
}

bool is_character(std::string_view path)
{
    return is_file_type(path, std::filesystem::file_type::character);
}

bool is_fifo(std::string_view path)
{
    return is_file_type(path, std::filesystem::file_type::fifo);
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
        SIHD_LOG(debug, "permission_get: {}: {}", ec.message(), path);
    return ec ? 0 : static_cast<unsigned int>(p);
}

// directories

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

bool make_directory(std::string_view path, unsigned int mode);

bool make_directories(std::string_view path, unsigned int mode)
{
    bool ret = true;
    if (!path.empty())
    {
        std::string separator = sep_str();
        Splitter splitter(separator);
        std::vector<std::string> dirnames = splitter.split(path);
        std::string current_path;
        size_t start = 0;
        if constexpr (sihd::util::build::is_windows)
        {
            // drive-absolute path "C:\..." -> first token "C:" is the root, not a dir to create
            if (!dirnames.empty() && dirnames[0].size() == 2 && dirnames[0][1] == ':')
            {
                current_path = dirnames[0] + separator;
                start = 1;
            }
            else if (path[0] == g_separator_char)
            {
                current_path = separator;
            }
        }
        else if (path[0] == g_separator_char)
        {
            current_path = separator;
        }
        for (size_t i = start; i < dirnames.size(); ++i)
        {
            current_path = combine(current_path, dirnames[i]);
            if (is_dir(current_path))
                continue;
            ret = make_directory(current_path, mode);
            if (ret == false)
                break;
        }
    }
    return ret;
}

// path manipulation

std::string normalize(std::string_view path)
{
    std::string separator = sep_str();
    Splitter splitter(separator);
    std::vector<std::string> splits = splitter.split(path);
    std::list<std::string> lst;

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

void trim_in_path(std::span<std::string> list, std::string_view to_remove)
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
    std::string ret = normalize(path);
    const size_t idx = ret.find_last_of(g_separator_char);
    return ret.substr(0, idx != std::string::npos ? idx : 0);
}

std::string combine(std::span<const char *> list)
{
    return combine_lst_impl(list);
}

std::string combine(std::initializer_list<std::string_view> list)
{
    return combine_lst_impl(list);
}

std::string combine(std::span<std::string_view> list)
{
    return combine_lst_impl(list);
}

std::string combine(std::span<const std::string> list)
{
    return combine_lst_impl(list);
}

std::string combine(std::string_view path1, std::string_view path2)
{
    return combine_impl(path1, path2);
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
    if constexpr (sihd::util::build::is_windows)
    {
        return (path.length() > 1 && path[0] == g_separator_char && path[1] == g_separator_char)
               || (path.length() > 2 && path[1] == ':' && path[2] == g_separator_char);
    }
    else
    {
        return path.length() > 0 && path[0] == g_separator_char;
    }
}

// files

std::optional<std::string> read_link(std::string_view path)
{
    std::error_code ec;
    std::filesystem::path link_path = std::filesystem::read_symlink(path, ec);
    if (ec)
    {
        // it is expected that some links are not readable, so only log if it is not a permission error
        if (ec != std::errc::permission_denied)
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
        SIHD_LOG(debug, "make_file_link: {}: {} -> {}", ec.message(), target, link);
    return ec.value() == 0;
}

bool make_dir_link(std::string_view target, std::string_view link)
{
    std::error_code ec;
    std::filesystem::create_directory_symlink(target, link, ec);
    if (ec)
        SIHD_LOG(debug, "make_dir_link: {}: {} -> {}", ec.message(), target, link);
    return ec.value() == 0;
}

bool make_hard_link(std::string_view target, std::string_view link)
{
    std::error_code ec;
    std::filesystem::create_hard_link(target, link, ec);
    if (ec)
        SIHD_LOG(debug, "make_hard_link: {}: {} -> {}", ec.message(), target, link);
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

bool rename(std::string_view from, std::string_view to)
{
    return ::rename(from.data(), to.data()) == 0;
}

std::string jail(std::string_view root_view, std::string_view path_view)
{
    if (root_view.empty())
        return std::string(path_view);

    // Normalize the root and drop trailing separators (keep a bare "/")
    std::string root = normalize(root_view);
    while (root.size() > 1 && root.back() == '/')
        root.pop_back();

    // Treat the client path as relative to the jail root
    std::string clean_path(path_view);
    if (!clean_path.empty() && clean_path[0] == '/')
        clean_path.erase(0, 1);

    // Lexically resolve '.'/'..' before touching the filesystem so create/write
    // targets (which don't exist yet, so realpath fails) are jailed too.
    // Avoid building a '//' prefix when root is "/" (normalize does not collapse it)
    std::string joined = (root == "/") ? ("/" + clean_path) : (root + "/" + clean_path);
    std::string candidate = normalize(joined);

    auto under_root = [&root](const std::string & p) {
        if (root == "/")
            return !p.empty() && p[0] == '/';
        return p == root || (p.size() > root.size() && p.compare(0, root.size(), root) == 0 && p[root.size()] == '/');
    };

    // Escape attempt (e.g. ../../etc/passwd): clamp to the jail root
    if (!under_root(candidate))
        return root;

    // If the target exists, resolve symlinks and re-check to catch links that
    // escape the jail; nonexistent targets keep the lexically-jailed path
    std::string real_str = realpath(candidate);
    if (!real_str.empty())
        return under_root(real_str) ? real_str : root;

    return candidate;
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
    return false;
}

std::optional<std::string> read(std::string_view path, sihd::util::Slice slice)
{
    File file(path, "r");
    if (!file.is_open())
        return std::nullopt;

    const size_t file_size = file.file_size();
    const auto range = slice.resolve(file_size);

    if (range.empty())
        return std::nullopt;

    if (!file.seek_begin(range.from))
        return std::nullopt;

    ssize_t ret;
    std::string str;
    if ((ret = file.read(str, range.size())) > 0)
    {
        return str;
    }

    return std::nullopt;
}

std::optional<std::string> read_line(std::string_view path, size_t line_number)
{
    std::ifstream file(path.data(), std::ifstream::in);
    if (file.is_open())
    {
        std::string line;
        size_t i = 0;
        while (i <= line_number)
        {
            if (file.eof() || file.bad() || !std::getline(file, line))
                return std::nullopt;
            ++i;
        }
        return line;
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

} // namespace sihd::sys::fs

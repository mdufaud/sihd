#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <fmt/ranges.h>

#include <sihd/sys/File.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/Splitter.hpp>
#include <sihd/util/build.hpp>
#include <sihd/util/str.hpp>

// home/exe paths, children iteration, truncation, mount/storage queries, chdir, tmp dirs: src/linux|windows/fs.cpp

using namespace sihd::util;
using enum sihd::util::ErrorCode;

namespace sihd::sys::fs
{

SIHD_NEW_LOGGER("sihd::sys::fs");

namespace
{

char g_separator_char = build::is_windows ? '\\' : '/';

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

bool is_under_root(const std::string & root, const std::string & path)
{
    if (root == "/")
        return !path.empty() && path[0] == '/';
    return path == root
           || (path.size() > root.size() && path.compare(0, root.size(), root) == 0 && path[root.size()] == '/');
}

Result<void> internal_set_perm(std::string_view path, unsigned int mode, std::filesystem::perm_options option)
{
    std::error_code ec;
    std::filesystem::permissions(path, static_cast<std::filesystem::perms>(mode), option, ec);
    if (ec)
    {
        return std::unexpected(Error(ec, "could not set permissions on '{}': {}", path, ec.message()));
    }
    return {};
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

Result<void> write_with_mode(const std::string & path, std::string_view view, const char *mode)
{
    File file;
    auto opened = file.open(path, mode);
    if (!opened)
        return opened;
    auto wrote = file.write(view);
    if (!wrote)
        SIHD_UNEXPECTED_RETURN(wrote);
    if (*wrote != view.size())
        return std::unexpected(Error(io_error, "short write to '{}'", path));
    return {};
}

} // namespace

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

bool is_file(const std::string & path)
{
    return is_file_type(path, std::filesystem::file_type::regular);
}

bool is_dir(const std::string & path)
{
    return is_file_type(path, std::filesystem::file_type::directory);
}

bool is_symlink(const std::string & path)
{
    std::error_code ec;
    return std::filesystem::symlink_status(path, ec).type() == std::filesystem::file_type::symlink;
}

bool is_socket(const std::string & path)
{
    return is_file_type(path, std::filesystem::file_type::socket);
}

bool is_block(const std::string & path)
{
    return is_file_type(path, std::filesystem::file_type::block);
}

bool is_character(const std::string & path)
{
    return is_file_type(path, std::filesystem::file_type::character);
}

bool is_fifo(const std::string & path)
{
    return is_file_type(path, std::filesystem::file_type::fifo);
}

std::string permission_to_str(unsigned int mode)
{
    using std::filesystem::perms;
    perms p = static_cast<perms>(mode);
    std::string ret;

    ret.reserve(9);
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

Result<void> permission_add(const std::string & path, unsigned int mode)
{
    return internal_set_perm(path, mode, std::filesystem::perm_options::add);
}

Result<void> permission_rm(const std::string & path, unsigned int mode)
{
    return internal_set_perm(path, mode, std::filesystem::perm_options::remove);
}

Result<void> permission_set(const std::string & path, unsigned int mode)
{
    return internal_set_perm(path, mode, std::filesystem::perm_options::replace);
}

unsigned int permission_get(const std::string & path)
{
    std::error_code ec;
    auto perms = std::filesystem::status(path, ec).permissions();
    if (ec)
        SIHD_LOG(debug, "permission_get: {}: {}", ec.message(), path);
    return ec ? 0 : static_cast<unsigned int>(perms);
}

Result<void> remove_directory(const std::string & path)
{
    if (rmdir(path.c_str()) != 0)
    {
        return std::unexpected(Error::from_errno("could not remove directory '{}'", path));
    }
    return {};
}

Result<void> remove_directories(const std::string & path)
{
    std::vector<std::string> children = recursive_children(path);
    Result<void> res = {};
    for (auto it = children.rbegin(); it != children.rend(); ++it)
    {
        auto removed = is_dir(*it) ? remove_directory(*it) : remove_file(*it);
        if (!removed && !res)
            res = std::move(removed);
    }
    return res;
}

Result<void> make_directory(const std::string & path, unsigned int mode);

Result<void> make_directories(const std::string & path, unsigned int mode)
{
    if (path.empty())
        return {};
    std::string separator = sep_str();
    Splitter splitter(separator);
    std::vector<std::string> dirnames = splitter.split(path);
    std::string current_path;
    size_t start = 0;

    if constexpr (build::is_windows)
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
        auto res = make_directory(current_path, mode);
        if (!res)
            return res;
    }
    return {};
}

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
    if constexpr (build::is_windows)
    {
        return (path.length() > 1 && path[0] == g_separator_char && path[1] == g_separator_char)
               || (path.length() > 2 && path[1] == ':' && path[2] == g_separator_char);
    }
    else
    {
        return path.length() > 0 && path[0] == g_separator_char;
    }
}

std::optional<std::string> read_link(const std::string & path)
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

Result<void> make_file_link(const std::string & target, const std::string & link)
{
    std::error_code ec;
    std::filesystem::create_symlink(target, link, ec);
    if (ec)
    {
        return std::unexpected(Error(ec, "could not create file link '{}' -> '{}': {}", link, target, ec.message()));
    }
    return {};
}

Result<void> make_dir_link(const std::string & target, const std::string & link)
{
    std::error_code ec;
    std::filesystem::create_directory_symlink(target, link, ec);
    if (ec)
    {
        return std::unexpected(
            Error(ec, "could not create directory link '{}' -> '{}': {}", link, target, ec.message()));
    }
    return {};
}

Result<void> make_hard_link(const std::string & target, const std::string & link)
{
    std::error_code ec;
    std::filesystem::create_hard_link(target, link, ec);
    if (ec)
    {
        return std::unexpected(Error(ec, "could not create hard link '{}' -> '{}': {}", link, target, ec.message()));
    }
    return {};
}

bool are_equals(const std::string & path1, const std::string & path2)
{
    File file1(path1, "rb");
    File file2(path2, "rb");

    if (!file1.is_open() || !file2.is_open())
        return false;

    const auto size1 = file1.file_size();
    const auto size2 = file2.file_size();
    if (!size1 || !size2 || *size1 != *size2)
        return false;

    constexpr size_t buffer_size = 4096;
    char buffer1[buffer_size];
    char buffer2[buffer_size];
    while (true)
    {
        const auto read1 = file1.read(buffer1, buffer_size);
        if (!read1)
            return false;
        if (*read1 == 0)
            break;
        const auto read2 = file2.read(buffer2, buffer_size);
        if (!read2 || *read2 != *read1)
            return false;
        if (::memcmp(buffer1, buffer2, *read1) != 0)
            return false;
    }
    return true;
}

Result<void> remove_file(const std::string & path)
{
    if (remove(path.c_str()) != 0)
    {
        return std::unexpected(Error::from_errno("could not remove file '{}'", path));
    }
    return {};
}

std::string jail(std::string_view root_view, std::string_view path_view)
{
    if (root_view.empty())
        return std::string(path_view);

    std::string root = normalize(root_view);
    while (root.size() > 1 && root.back() == '/')
        root.pop_back();

    std::string clean_path(path_view);
    if (!clean_path.empty() && clean_path[0] == '/')
        clean_path.erase(0, 1);

    // lexical resolve first so not-yet-existing targets are jailed; no '//' prefix when root is "/"
    std::string joined = (root == "/") ? ("/" + clean_path) : (root + "/" + clean_path);
    std::string candidate = normalize(joined);

    if (!is_under_root(root, candidate))
        return root;

    // resolve symlinks and re-check to catch escaping links; nonexistent targets keep the lexical path
    std::string real_str = realpath(candidate);
    if (!real_str.empty())
        return is_under_root(root, real_str) ? real_str : root;

    return candidate;
}

Result<void> write(const std::string & path, std::string_view view, bool append)
{
    return write_with_mode(path, view, append ? "a" : "w");
}

Result<void> write_binary(const std::string & path, std::string_view view, bool append)
{
    return write_with_mode(path, view, append ? "ab" : "wb");
}

Result<std::string> read(const std::string & path, Slice slice, bool binary)
{
    File file;
    auto opened = file.open(path, binary ? "rb" : "r");
    if (!opened)
        SIHD_UNEXPECTED_RETURN(opened);

    const auto size = file.file_size();
    if (!size)
        SIHD_UNEXPECTED_RETURN(size);
    const auto range = slice.resolve((size_t)*size);

    if (range.empty())
        return std::string {};

    if (!file.seek_begin(range.from))
        return std::unexpected(Error::from_errno("could not seek '{}' to {}", path, range.from));

    std::string str;
    const auto got = file.read(str, range.size());
    if (!got)
        SIHD_UNEXPECTED_RETURN(got);
    if (*got == 0)
        return std::unexpected(Error(io_error, "could not read '{}' at offset {}", path, range.from));
    return str;
}

Result<std::string> read_line(const std::string & path, size_t line_number)
{
    std::ifstream file(path.c_str(), std::ifstream::in);
    if (!file.is_open())
        return std::unexpected(Error::from_errno("could not open '{}'", path));
    std::string line;
    size_t i = 0;
    while (i <= line_number)
    {
        if (file.eof() || file.bad() || !std::getline(file, line))
            return std::unexpected(Error(not_found, "no line {} in '{}'", line_number, path));
        ++i;
    }
    return line;
}

Result<std::vector<std::string>> read_lines(const std::string & path)
{
    return read_all(path).transform([](const std::string & content) { return str::split(content, '\n'); });
}

Result<std::string> read_all(const std::string & path, bool binary)
{
    const std::ios_base::openmode mode = binary ? (std::ifstream::in | std::ifstream::binary) : std::ifstream::in;
    std::ifstream file(path.c_str(), mode);
    if (!file.is_open() || !file.good())
        return std::unexpected(Error::from_errno("could not open '{}'", path));
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    if (file.bad())
        return std::unexpected(Error(io_error, "could not read '{}'", path));
    return buffer.str();
}

ssize_t read_binary(const std::string & path, char *buf, size_t size)
{
    File file(path, "rb");

    if (file.is_open())
    {
        const auto read = file.read(buf, size);
        if (read)
            return (ssize_t)*read;
    }
    return -1;
}

} // namespace sihd::sys::fs

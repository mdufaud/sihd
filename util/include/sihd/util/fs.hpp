#ifndef __SIHD_UTIL_FS_HPP__
#define __SIHD_UTIL_FS_HPP__

#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/util/Timestamp.hpp>

namespace sihd::util::fs
{

// utils
char sep();
std::string sep_str();
void set_sep(char c);

std::string tmp_path();
std::string home_path();
std::string cwd();
std::string executable_path();

// stat
bool exists(std::string_view path);
bool is_file(std::string_view path);
bool is_dir(std::string_view path);
size_t file_size(std::string_view path);
Timestamp last_write(std::string_view path);
bool is_readable(std::string_view path);
bool is_writable(std::string_view path);
bool is_executable(std::string_view path);

// directories
std::string make_tmp_directory(std::string_view prefix = "");
bool remove_directory(std::string_view path);
bool remove_directories(std::string_view path);
bool make_directory(std::string_view path, unsigned int mode = 0750);
bool make_directories(std::string_view path, unsigned int mode = 0750);
std::vector<std::string> children(std::string_view path);
std::vector<std::string> recursive_children(std::string_view path, uint32_t max_depth = 0);

// path manipulation
bool is_absolute(std::string_view path);
std::string normalize(std::string_view path);
std::string trim_path(std::string_view path, std::string_view to_remove);
void trim_in_path(std::string & path, std::string_view to_remove);
void trim_in_path(std::vector<std::string> & list, std::string_view to_remove);

std::string parent(std::string_view path);
std::string filename(std::string_view path);
std::string extension(std::string_view path);

std::string combine(std::initializer_list<std::string_view> list);
std::string combine(const std::vector<std::string> & list);
// if path2 is empty, it will append the separation character to path1
std::string combine(std::string_view path1, std::string_view path2);

// ensure separating char is at the end of path
std::string ensure_separation(std::string_view path);

// files
bool remove_file(std::string_view path);
bool are_equals(std::string_view path1, std::string_view path2);

// links
bool make_file_link(std::string_view target, std::string_view link);
bool make_dir_link(std::string_view target, std::string_view link);
bool make_hard_link(std::string_view target, std::string_view link);
std::optional<std::string> read_link(std::string_view path);

// fast read from file
ssize_t read_binary(std::string_view path, char *buf, size_t size);
// fast string read from file
std::optional<std::string> read(std::string_view path, size_t size);
// fast all file read
std::optional<std::string> read_all(std::string_view path);

// fast binary write into file
bool write_binary(std::string_view path, std::string_view view, bool append = false);
// fast write into file
bool write(std::string_view path, std::string_view view, bool append = false);

} // namespace sihd::util::fs

#endif
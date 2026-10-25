#ifndef __SIHD_SYS_FS_HPP__
#define __SIHD_SYS_FS_HPP__

#include <functional>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/util/Result.hpp>
#include <sihd/util/Slice.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/ZStringView.hpp>

namespace sihd::sys::fs
{

// creation is epoch 0 when the backing filesystem/kernel doesn't track birth times
struct FileTimes
{
        sihd::util::Timestamp creation;
        sihd::util::Timestamp access;
        sihd::util::Timestamp write;
};

enum class MountType
{
    local,
    network,
    ram,
    readonly,
    unknown,
};
enum class StorageMedium
{
    ssd,
    hdd,
    unknown,
};

struct MountEntry
{
        std::string source;
        std::string mount_point;
        std::string fs_type;
        MountType type = MountType::unknown;
};

char sep();
std::string sep_str();
void set_sep(char c);

std::string tmp_path();
std::string home_path();
std::string cwd();

// XDG base-dir spec on linux, SHGetKnownFolders on windows; no directory is created
std::string config_path();
std::string data_path();
std::string cache_path();
std::string download_path();

bool is_file(std::string_view path);
bool is_dir(std::string_view path);
bool is_symlink(std::string_view path);
bool is_socket(std::string_view path);
bool is_block(std::string_view path);
bool is_character(std::string_view path);
bool is_fifo(std::string_view path);
sihd::util::Result<size_t> file_size(sihd::util::ZStringView path);

// creation is epoch 0 when the backing filesystem/kernel doesn't track birth times
sihd::util::Result<FileTimes> times(sihd::util::ZStringView path);
sihd::util::Timestamp last_write(sihd::util::ZStringView path);

bool exists(sihd::util::ZStringView path);
bool is_readable(sihd::util::ZStringView path);
bool is_writable(sihd::util::ZStringView path);
bool is_executable(sihd::util::ZStringView path);

MountType mount_type(const std::string & path);
// heuristic (rotational vs seek-penalty); unknown for network/ram/readonly/virtual/unresolvable backings
StorageMedium storage_medium(const std::string & path);

std::vector<MountEntry> mounts();
sihd::util::Result<uint64_t> free_space(const std::string & path);
sihd::util::Result<uint64_t> total_space(const std::string & path);

std::string permission_to_str(unsigned int mode);
unsigned int permission_from_str(std::string_view mode);
sihd::util::Result<void> permission_add(const std::string & path, unsigned int mode);
sihd::util::Result<void> permission_rm(const std::string & path, unsigned int mode);
sihd::util::Result<void> permission_set(const std::string & path, unsigned int mode);
unsigned int permission_get(const std::string & path);

sihd::util::Result<std::string> make_tmp_directory(const std::string & prefix = "");
sihd::util::Result<void> remove_directory(sihd::util::ZStringView path);
sihd::util::Result<void> remove_directories(const std::string & path);
sihd::util::Result<void> make_directory(sihd::util::ZStringView path, unsigned int mode = 0750);
sihd::util::Result<void> make_directories(const std::string & path, unsigned int mode = 0750);
std::vector<std::string> children(const std::string & path);
std::vector<std::string> recursive_children(const std::string & path, uint32_t max_depth = 0);

bool is_absolute(std::string_view path);
std::string normalize(std::string_view path);
std::string trim_path(std::string_view path, std::string_view to_remove);
void trim_in_path(std::string & path, std::string_view to_remove);
void trim_in_path(std::span<std::string> list, std::string_view to_remove);

std::string parent(std::string_view path);
std::string filename(std::string_view path);
std::string extension(std::string_view path);

std::string combine(std::initializer_list<std::string_view> list);
std::string combine(std::span<std::string_view> list);
std::string combine(std::span<const std::string> list);
std::string combine(std::span<const char *> list);
// if path2 is empty, it will append the separation character to path1
std::string combine(std::string_view path1, std::string_view path2);

std::string ensure_separation(std::string_view path);

sihd::util::Result<void> remove_file(sihd::util::ZStringView path);
sihd::util::Result<void> rename(sihd::util::ZStringView from, sihd::util::ZStringView to);
sihd::util::Result<void> truncate(sihd::util::ZStringView path, int64_t size);
bool are_equals(const std::string & path1, const std::string & path2);

// overwrites 'to'; false from 'progress' cancels and removes the partial 'to', a failed start leaves it untouched
sihd::util::Result<void> copy_file(const std::string & from,
                                   const std::string & to,
                                   const std::function<bool(size_t transferred, size_t total)> & progress = nullptr);

std::string realpath(sihd::util::ZStringView path);

// collapses '.'/'..' lexically, then resolves symlinks; escapes clamp to 'root'; empty root = unchanged
std::string jail(std::string_view root, std::string_view path);

sihd::util::Result<void> make_file_link(const std::string & target, const std::string & link);
sihd::util::Result<void> make_dir_link(const std::string & target, const std::string & link);
sihd::util::Result<void> make_hard_link(const std::string & target, const std::string & link);
std::optional<std::string> read_link(const std::string & path);

ssize_t read_binary(const std::string & path, char *buf, size_t size);
// fast string read: offset > 0 seeks from the beginning, offset < 0 from the end
sihd::util::Result<std::string> read(const std::string & path, sihd::util::Slice slice, bool binary = true);
sihd::util::Result<std::string> read_line(sihd::util::ZStringView path, size_t line_number);
// text mode only; empty lines are omitted
sihd::util::Result<std::vector<std::string>> read_lines(const std::string & path);
// the default text mode stops at ^Z on windows: pass binary=true for raw bytes
sihd::util::Result<std::string> read_all(sihd::util::ZStringView path, bool binary = false);

// fast write into file; text mode translates LF to CRLF on windows
sihd::util::Result<void> write(const std::string & path, std::string_view view, bool append = false);
sihd::util::Result<void> write_binary(const std::string & path, std::string_view view, bool append = false);

sihd::util::Result<void> chdir(sihd::util::ZStringView path);

} // namespace sihd::sys::fs

#endif
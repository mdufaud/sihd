#ifndef __SIHD_UTIL_FS_HPP__
# define __SIHD_UTIL_FS_HPP__

# include <sihd/util/Str.hpp> // Str utils, vector, sstream, string
# include <sihd/util/ArrayView.hpp>
# include <optional>
# include <fstream>

namespace sihd::util
{

class FS
{
    public:
        // utils
        static std::string tmp_path();
        static std::string home_path();
        static std::string cwd();
        static std::string executable_path();

        // stat
        static bool exists(std::string_view path);
        static bool is_file(std::string_view path);
        static bool is_dir(std::string_view path);
        static size_t filesize(std::string_view path);
        static time_t last_write(std::string_view path);
        static bool is_readable(std::string_view path);
        static bool is_writable(std::string_view path);
        static bool is_executable(std::string_view path);

        // directories
        static std::string make_tmp_directory(std::string_view prefix = "");
        static bool remove_directory(std::string_view path);
        static bool remove_directories(std::string_view path);
        static bool make_directory(std::string_view path, mode_t mode = 0750);
        static bool make_directories(std::string_view path, mode_t mode = 0750);
        static std::vector<std::string> children(std::string_view path);
        static std::vector<std::string> recursive_children(std::string_view path);

        // path manipulation
        static bool is_absolute(std::string_view path);
        static std::string normalize(std::string_view path);
        static std::string trim_path(std::string_view path, std::string_view to_remove);
        static void trim_in_path(std::string & path, std::string_view to_remove);
        static void trim_in_path(std::vector<std::string> & list, std::string_view to_remove);

        static std::string parent(std::string_view path);
        static std::string filename(std::string_view path);
        static std::string extension(std::string_view path);

        static std::string combine(std::initializer_list<std::string_view> list);
        static std::string combine(const std::vector<std::string> & list);
        // if path2 is empty, it will append the separation character to path1
        static std::string combine(std::string_view path1, std::string_view path2);

        // ensure separating char is at the end of path
        static std::string ensure_separation(std::string_view path);

        // files
        static bool remove_file(std::string_view path);
        static bool are_equals(std::string_view path1, std::string_view path2);

        // fast read from file
        static ssize_t read_binary(std::string_view path, char *buf, size_t size);
        // fast string read from file
        static std::optional<std::string> read(std::string_view path, size_t size);
        // fast all file read
        static std::optional<std::string> read_all(std::string_view path);

        // fast binary write into file
        static bool write_binary(std::string_view path, ArrViewChar view, bool append = false);
        // fast write into file
        static bool write(std::string_view path, ArrViewChar view, bool append = false);

        inline static std::string sep_str() { return std::string(1, FS::sep); };
        static char sep;

    protected:

    private:
        virtual ~FS() {};

        static std::string _combine(std::string_view path1, std::string_view path2);

# if !defined(__SIHD_WINDOWS__)
        static void _get_recursive_children(std::string_view path, std::vector<std::string> & children);
# endif
};

}

#endif
#ifndef __SIHD_UTIL_FILES_HPP__
# define __SIHD_UTIL_FILES_HPP__

# include <sihd/util/Str.hpp> // Str utils, vector, sstream, string
# include <sihd/util/IArray.hpp>
# include <optional>
# include <fstream>

namespace sihd::util
{

class Files
{
    public:
        // stat
        static bool exists(const std::string & path);
        static bool is_file(const std::string & path);
        static bool is_dir(const std::string & path);
        static size_t get_filesize(const std::string & path);

        // directories
        static bool remove_directory(const std::string & path);
        static bool remove_directories(const std::string & path);
        static bool make_directory(const std::string & path, mode_t mode = 0750);
        static bool make_directories(const std::string & path, mode_t mode = 0750);
        static std::vector<std::string> get_children(const std::string & path);
        static std::vector<std::string> get_recursive_children(const std::string & path);

        // path manipulation
        static bool is_absolute(const std::string & path);
        static std::string normalize(const std::string & path);
        static std::string trim_path(std::string && path, const std::string & to_remove);
        static std::string trim_path(const std::string & path, const std::string & to_remove);
        static void trim_in_path(std::string & path, const std::string & to_remove);
        static void trim_in_path(std::vector<std::string> & list, const std::string & to_remove);
        static std::string get_parent(const std::string & path);
        static std::string get_filename(const std::string & path);
        static std::string get_extension(const std::string & path);
        static std::string combine(std::vector<std::string> && list);
        static std::string combine(const std::vector<std::string> & list);
        // if path2 is empty, it will append the separation character to path1
        static std::string combine(const std::string & path1, const std::string & path2);

        // files
        static bool remove_file(const std::string & path);
        static bool are_equals(const std::string & path1, const std::string & path2);

        // fast read from file
        static ssize_t read_binary(const std::string & path, char *buf, size_t size);
        // fast string read from file
        static std::optional<std::string> read(const std::string & path, size_t size);
        // fast all file read
        static std::optional<std::string> read_all(const std::string & path);

        // fast binary write into file
        static bool write_binary(const std::string & path, const char *data, size_t size, bool append = false);
        // fast write into file
        static bool write(const std::string & path, const std::string & content, bool append = false);

        inline static std::string sep_str() { return std::string(1, Files::sep); };
        static char sep;

    protected:

    private:
        Files() {};
        virtual ~Files() {};

# if !defined(__SIHD_WINDOWS__)
        static void _get_recursive_children(const std::string & path, std::vector<std::string> & children);
# endif
};

}

#endif
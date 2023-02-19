#ifndef __SIHD_UTIL_TMPDIR_HPP__
# define __SIHD_UTIL_TMPDIR_HPP__

# include <string>
# include <string_view>

namespace sihd::util
{

class TmpDir
{
    public:
        // creates a directory in constructor
        TmpDir(std::string_view path);
        // automatically get a tmp path prefix
        TmpDir();
        // deletes the directory in destructor
        ~TmpDir();

        operator bool() const;

        const std::string & path() const;

    private:
        std::string _path;
};

}

#endif
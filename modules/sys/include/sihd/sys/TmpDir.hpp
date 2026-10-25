#ifndef __SIHD_SYS_TMPDIR_HPP__
#define __SIHD_SYS_TMPDIR_HPP__

#include <string>
#include <string_view>

#include <sihd/util/ZStringView.hpp>

namespace sihd::sys
{

class TmpDir
{
    public:
        // creates a directory in constructor
        TmpDir();
        // deletes the directory in destructor
        ~TmpDir();

        TmpDir(const TmpDir &) = delete;
        TmpDir & operator=(const TmpDir &) = delete;

        operator bool() const;
        operator std::string() const;
        operator std::string_view() const;
        operator sihd::util::ZStringView() const;

        const std::string & path() const;

    private:
        std::string _path;
};

} // namespace sihd::sys

#endif

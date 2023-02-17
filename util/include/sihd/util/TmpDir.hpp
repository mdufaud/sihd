#ifndef __SIHD_UTIL_TMPDIR_HPP__
# define __SIHD_UTIL_TMPDIR_HPP__

# include <sihd/util/fs.hpp>

namespace sihd::util
{

class TmpDir
{
    public:
        TmpDir(std::string_view prefix = fs::ensure_separation(fs::tmp_path()))
        {
            _path = fs::make_tmp_directory(prefix);
        }

        ~TmpDir()
        {
            if (fs::is_dir(_path))
            {
                if (fs::remove_directories(_path))
                    fs::remove_directory(_path);
            }
        };

        operator bool() const { return _path.empty() == false; }

        const std::string & path() const { return _path; }

    private:
        std::string _path;
};

}

#endif
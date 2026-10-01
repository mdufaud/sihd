#ifndef __SIHD_PY_DIRECTORYSWITCHER_HPP__
#define __SIHD_PY_DIRECTORYSWITCHER_HPP__

#include <unistd.h>

#include <cstdlib>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

namespace test
{

// the generated test runner exports LIB_PATH: a bare binary run cannot locate the scripts
inline std::string_view lib_path()
{
    const char *path = getenv("LIB_PATH");
    if (path == nullptr)
    {
        ADD_FAILURE() << "LIB_PATH is not set: run the tests through the generated runner (make itest)";
        return {};
    }
    return path;
}

class DirectorySwitcher
{
    public:
        DirectorySwitcher(std::string_view path);
        virtual ~DirectorySwitcher();

        const std::string & cwd() const { return _cwd; }
        const std::string & old_cwd() const { return _old_cwd; }

    protected:

    private:
        std::string _cwd;
        std::string _old_cwd;
};

} // namespace test

#endif
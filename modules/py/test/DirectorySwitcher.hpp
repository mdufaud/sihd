#ifndef __SIHD_PY_DIRECTORYSWITCHER_HPP__
#define __SIHD_PY_DIRECTORYSWITCHER_HPP__

#include <string>
#include <string_view>
#include <unistd.h>

namespace test
{

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
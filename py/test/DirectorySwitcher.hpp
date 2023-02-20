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

    protected:

    private:
        std::string _old_cwd;
};

} // namespace test

#endif
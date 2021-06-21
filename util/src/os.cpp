#include <sihd/util/os.hpp>
#include <vector>

namespace sihd::util::os
{

void    *load_lib(std::string lib_name)
{
    std::vector<std::string>    to_try = {
        "lib" + lib_name + ".so",
        lib_name + ".so",
        lib_name,
        lib_name + ".dll"
    };
    void *handle = nullptr;
    for (const std::string & lib : to_try)
    {
        handle = dlopen(lib.c_str(), RTLD_NOW);
        if (handle != nullptr)
            break ;
    }
    return handle;
}

}
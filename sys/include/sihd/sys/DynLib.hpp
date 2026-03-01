#ifndef __SIHD_SYS_DYNLIB_HPP__
#define __SIHD_SYS_DYNLIB_HPP__

#include <string>
#include <string_view>

namespace sihd::sys
{

class DynLib
{
    public:
        DynLib();
        DynLib(std::string_view lib_name);
        ~DynLib();

        bool open(std::string_view lib_name);

        void *load(std::string_view symbol_name);

        bool close();

        bool is_open() const { return _handle != nullptr; }

    protected:

    private:
        void *_handle;
        std::string _name;
};

} // namespace sihd::sys

#endif

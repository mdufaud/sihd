#ifndef __SIHD_LUA_LUAZLIBAPI_HPP__
#define __SIHD_LUA_LUAZLIBAPI_HPP__

// clang-format off
#include <sihd/lua/Vm.hpp>
// clang-format on

#include <sihd/zlib/Deflater.hpp>

template <>
struct luabridge::Stack<sihd::zlib::Deflater::Format>: luabridge::Enum<sihd::zlib::Deflater::Format,
                                                                       sihd::zlib::Deflater::Format::zlib,
                                                                       sihd::zlib::Deflater::Format::gzip>
{
};

namespace sihd::lua
{

class LuaZlibApi
{
    public:
        static void load_all(Vm & vm);

        static void load_base(Vm & vm);

    private:
        LuaZlibApi() = delete;
        ~LuaZlibApi() = delete;
};

} // namespace sihd::lua

#endif

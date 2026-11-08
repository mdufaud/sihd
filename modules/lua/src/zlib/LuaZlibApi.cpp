#include <exception>
#include <stdexcept>
#include <string>

// clang-format off
#include <sihd/lua/zlib/LuaZlibApi.hpp>
#include <sihd/lua/util/LuaUtilApi.hpp>
#include <sihd/lua/checked.hpp>
// clang-format on

#include <sihd/core/Device.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/SmartNodePtr.hpp>
#include <sihd/zlib/DevDeflate.hpp>
#include <sihd/zlib/DevInflate.hpp>
#include <sihd/zlib/Inflater.hpp>

namespace sihd::lua
{

using namespace sihd::zlib;
using namespace sihd::util;
using sihd::core::Device;

namespace
{

Error error_of(const std::exception & exception)
{
    const bool invalid = dynamic_cast<const std::invalid_argument *>(&exception) != nullptr;
    return Error(invalid ? ErrorCode::invalid_argument : ErrorCode::unknown, "{}", exception.what());
}

// every C++ local dies in here: the factory raises from a frame that only holds the deflater pointer
int new_deflater(lua_State *state, Deflater *& deflater)
{
    try
    {
        if (lua_isnoneornil(state, 2))
        {
            deflater = new Deflater();
            return 0;
        }

        auto format = luabridge::Stack<Deflater::Format>::get(state, 2);
        if (!format)
            return push_error(state, Error(ErrorCode::invalid_argument, "argument 1: {}", format.message()));
        if (lua_isnoneornil(state, 3))
        {
            deflater = new Deflater(*format);
            return 0;
        }

        auto level = luabridge::Stack<int>::get(state, 3);
        if (!level)
            return push_error(state, Error(ErrorCode::invalid_argument, "argument 2: {}", level.message()));
        deflater = new Deflater(*format, *level);
        return 0;
    }
    catch (const std::exception & exception)
    {
        return push_error(state, error_of(exception));
    }
}

int new_inflater(lua_State *state, Inflater *& inflater)
{
    auto max_output_size = luabridge::Stack<size_t>::get(state, 2);
    if (!max_output_size)
        return push_error(state, Error(ErrorCode::invalid_argument, "argument 1: {}", max_output_size.message()));
    try
    {
        inflater = new Inflater(*max_output_size);
        return 0;
    }
    catch (const std::exception & exception)
    {
        return push_error(state, error_of(exception));
    }
}

std::string string_of(const ArrByte & bytes)
{
    return std::string(bytes.cpp_str_view());
}

} // namespace

void LuaZlibApi::load_all(Vm & vm)
{
    LuaZlibApi::load_base(vm);
}

void LuaZlibApi::load_base(Vm & vm)
{
    luabridge::getGlobalNamespace(vm.lua_state())
        .beginNamespace("sihd")
        .beginNamespace("zlib")
        .beginClass<Deflater>("Deflater")
        .addFactory(
            +[](lua_State *L) -> Deflater * {
                Deflater *deflater = nullptr;
                if (new_deflater(L, deflater) < 0)
                    lua_error(L);
                return deflater;
            },
            +[](Deflater *deflater) { delete deflater; })
        .addStaticProperty("Format",
                           [](lua_State *state) {
                               luabridge::LuaRef formats = luabridge::newTable(state);
                               formats["zlib"] = Deflater::Format::zlib;
                               formats["gzip"] = Deflater::Format::gzip;
                               return formats;
                           })
        .addFunction("deflate_stream", checked<+[](Deflater *self, const std::string & in) {
                         return self->deflate_stream(ArrByteView(in)).transform(string_of);
                     }>)
        .addFunction("deflate_message", checked<+[](Deflater *self, const std::string & in) {
                         return self->deflate_message(ArrByteView(in)).transform(string_of);
                     }>)
        .addFunction("output_bound", &Deflater::output_bound)
        .endClass()
        .beginClass<Inflater>("Inflater")
        .addFactory(
            +[](lua_State *L) -> Inflater * {
                Inflater *inflater = nullptr;
                if (new_inflater(L, inflater) < 0)
                    lua_error(L);
                return inflater;
            },
            +[](Inflater *inflater) { delete inflater; })
        .addFunction("inflate_stream", checked<+[](Inflater *self, const std::string & in) {
                         return self->inflate_stream(ArrByteView(in)).transform(string_of);
                     }>)
        .addFunction("inflate_message", checked<+[](Inflater *self, const std::string & in) {
                         return self->inflate_message(ArrByteView(in)).transform(string_of);
                     }>)
        .endClass()
        // devices (derive from core Device: core must be loaded first)
        .deriveClass<DevDeflate, Device>("DevDeflate")
        .addConstructorFrom<SmartNodePtr<DevDeflate>, void(const std::string &, Node *)>()
        .addFunction("set_mode", &DevDeflate::set_mode)
        .addFunction("set_level", &DevDeflate::set_level)
        .addFunction("set_format", &DevDeflate::set_format)
        .endClass()
        .deriveClass<DevInflate, Device>("DevInflate")
        .addConstructorFrom<SmartNodePtr<DevInflate>, void(const std::string &, Node *)>()
        .addFunction("set_mode", &DevInflate::set_mode)
        .addFunction("set_output_capacity", &DevInflate::set_output_capacity)
        .endClass()
        .endNamespace()
        .endNamespace();
}

} // namespace sihd::lua

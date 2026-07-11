#include <cmath>
#include <limits>

#include <sihd/json/Json.hpp>
#include <sihd/lua/json/LuaJsonApi.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::lua
{

using namespace sihd::json;
using namespace sihd::util;
SIHD_NEW_LOGGER("sihd::lua::json");

namespace
{

luabridge::LuaRef json_to_lua(lua_State *state, const Json & json)
{
    switch (json.type())
    {
        case Json::Type::Bool:
            return luabridge::LuaRef(state, json.get<bool>());
        case Json::Type::Int:
            return luabridge::LuaRef(state, json.get<int64_t>());
        case Json::Type::Uint:
            return luabridge::LuaRef(state, json.get<uint64_t>());
        case Json::Type::Float:
            return luabridge::LuaRef(state, json.get<double>());
        case Json::Type::String:
            return luabridge::LuaRef(state, json.get<std::string>());
        case Json::Type::Array:
        {
            luabridge::LuaRef table = luabridge::newTable(state);
            const size_t size = json.size();
            for (size_t i = 0; i < size; ++i)
                table[static_cast<int>(i + 1)] = json_to_lua(state, json[i]);
            return table;
        }
        case Json::Type::Object:
        {
            luabridge::LuaRef table = luabridge::newTable(state);
            for (auto it = json.begin(); it != json.end(); ++it)
                table[it.key()] = json_to_lua(state, it.value());
            return table;
        }
        case Json::Type::Null:
        case Json::Type::Discarded:
        default:
            return luabridge::LuaRef(state, luabridge::LuaNil());
    }
}

Json lua_to_json(const luabridge::LuaRef & ref)
{
    switch (ref.type())
    {
        case LUA_TBOOLEAN:
            return Json(static_cast<bool>(ref));
        case LUA_TNUMBER:
        {
            const double value = static_cast<double>(ref);
            if (std::trunc(value) == value && !std::isinf(value)
                && value >= static_cast<double>(std::numeric_limits<int64_t>::min())
                && value <= static_cast<double>(std::numeric_limits<int64_t>::max()))
            {
                return Json(static_cast<int64_t>(value));
            }
            return Json(value);
        }
        case LUA_TSTRING:
            return Json(std::string(ref));
        case LUA_TTABLE:
        {
            size_t count = 0;
            bool sequence = true;
            for (const auto & pair : luabridge::pairs(ref))
            {
                ++count;
                if (pair.first.type() != LUA_TNUMBER)
                    sequence = false;
            }
            const size_t len = static_cast<size_t>(ref.length());
            // pure 1..N integer-keyed table -> JSON array, everything else -> object
            if (sequence && count > 0 && count == len)
            {
                Json::Array arr;
                arr.reserve(len);
                for (size_t i = 1; i <= len; ++i)
                {
                    luabridge::LuaRef value = ref[static_cast<int>(i)];
                    arr.push_back(lua_to_json(value));
                }
                return Json(std::move(arr));
            }
            Json::Object obj;
            for (const auto & pair : luabridge::pairs(ref))
                obj.emplace_back(pair.first.tostring(), lua_to_json(pair.second));
            return Json(std::move(obj));
        }
        case LUA_TNIL:
        default:
            return Json(nullptr);
    }
}

luabridge::LuaRef json_decode(const std::string & str, lua_State *state)
{
    Json json = Json::parse(str, false);
    if (json.is_discarded())
    {
        SIHD_LOG(error, "LuaJsonApi: failed to parse JSON string");
        return luabridge::LuaRef(state, luabridge::LuaNil());
    }
    return json_to_lua(state, json);
}

std::string json_encode(luabridge::LuaRef value, luabridge::LuaRef indent)
{
    const int indent_value = indent.isNil() ? -1 : static_cast<int>(indent);
    return lua_to_json(value).dump(indent_value);
}

} // namespace

void LuaJsonApi::load_all(Vm & vm)
{
    LuaJsonApi::load_base(vm);
}

void LuaJsonApi::load_base(Vm & vm)
{
    luabridge::getGlobalNamespace(vm.lua_state())
        .beginNamespace("sihd")
        .beginNamespace("json")
        .addFunction("decode", &json_decode)
        .addFunction("parse", &json_decode)
        .addFunction("encode", &json_encode)
        .addFunction("dump", &json_encode)
        .endNamespace()
        .endNamespace();
}

} // namespace sihd::lua

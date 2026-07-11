#ifndef __SIHD_LUA_HPP__
#define __SIHD_LUA_HPP__

// A binding is only compiled when its module is in the build, so each api header
// is guarded by the matching define from the generated config header. Calling an
// api that was not built is then a compile error instead of a link error.

// clang-format off
#include <sihd/lua/config.hpp>
#include <sihd/lua/Vm.hpp>

#if SIHD_LUA_WITH_UTIL
# include <sihd/lua/util/LuaUtilApi.hpp>
#endif
#if SIHD_LUA_WITH_SYS
# include <sihd/lua/sys/LuaSysApi.hpp>
#endif
#if SIHD_LUA_WITH_CORE
# include <sihd/lua/core/LuaCoreApi.hpp>
#endif
#if SIHD_LUA_WITH_JSON
# include <sihd/lua/json/LuaJsonApi.hpp>
#endif
#if SIHD_LUA_WITH_CSV
# include <sihd/lua/csv/LuaCsvApi.hpp>
#endif
#if SIHD_LUA_WITH_ZIP
# include <sihd/lua/zip/LuaZipApi.hpp>
#endif
#if SIHD_LUA_WITH_NET
# include <sihd/lua/net/LuaNetApi.hpp>
#endif
#if SIHD_LUA_WITH_HTTP
# include <sihd/lua/http/LuaHttpApi.hpp>
#endif
// clang-format on

#endif

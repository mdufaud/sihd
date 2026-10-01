#include <iostream>

#include <gtest/gtest.h>

#include <sihd/lua/Vm.hpp>
#include <sihd/lua/util/LuaUtilApi.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/term.hpp>

#include "../lua_fixture.hpp"

namespace test
{
SIHD_NEW_LOGGER("test");
using namespace sihd::util;
using namespace sihd::lua;
class TestLuaUtilApi: public test::LuaFixture
{
};

TEST_F(TestLuaUtilApi, test_luautil_node)
{
    LuaUtilApi::load_base(_vm);
    this->do_script("test/util/lua/test_node.lua");
}

TEST_F(TestLuaUtilApi, test_luautil_array)
{
    LuaUtilApi::load_base(_vm);
    this->do_script("test/util/lua/test_array.lua");
}

TEST_F(TestLuaUtilApi, test_luautil_log)
{
    LuaUtilApi::load_base(_vm);
    this->do_script("test/util/lua/test_log.lua");
}

TEST_F(TestLuaUtilApi, test_luautil_tools)
{
    LuaUtilApi::load_base(_vm);
    LuaUtilApi::load_tools(_vm);
    this->do_script("test/util/lua/test_tools.lua");
}

TEST_F(TestLuaUtilApi, test_luautil_timestamp)
{
    LuaUtilApi::load_base(_vm);
    LuaUtilApi::load_tools(_vm);
    this->do_script("test/util/lua/test_timestamp.lua");
}

TEST_F(TestLuaUtilApi, test_luautil_duration)
{
    LuaUtilApi::load_base(_vm);
    LuaUtilApi::load_tools(_vm);
    this->do_script("test/util/lua/test_duration.lua");
}

TEST_F(TestLuaUtilApi, test_luautil_splitter)
{
    LuaUtilApi::load_base(_vm);
    LuaUtilApi::load_tools(_vm);
    this->do_script("test/util/lua/test_splitter.lua");
}

TEST_F(TestLuaUtilApi, test_luautil_thread)
{
    LuaUtilApi::load_base(_vm);
    LuaUtilApi::load_tools(_vm);
    LuaUtilApi::load_threading(_vm);
    this->do_script("test/util/lua/test_thread.lua");
}

TEST_F(TestLuaUtilApi, test_luautil_errors)
{
    LuaUtilApi::load_base(_vm);
    LuaUtilApi::load_tools(_vm);
    LuaUtilApi::load_threading(_vm);
    this->do_script("test/util/lua/test_errors.lua");
}

TEST_F(TestLuaUtilApi, test_luautil_ownership)
{
    LuaUtilApi::load_base(_vm);
    LuaUtilApi::load_tools(_vm);
    LuaUtilApi::load_threading(_vm);
    this->do_script("test/util/lua/test_ownership.lua");
}

TEST_F(TestLuaUtilApi, test_luautil_thread_stress)
{
    LuaUtilApi::load_base(_vm);
    LuaUtilApi::load_tools(_vm);
    LuaUtilApi::load_threading(_vm);
    this->do_script("test/util/lua/test_thread_stress.lua");
}

} // namespace test
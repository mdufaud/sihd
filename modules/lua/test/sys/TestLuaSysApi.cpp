#include <iostream>

#include <gtest/gtest.h>

#include <sihd/lua/Vm.hpp>
#include <sihd/lua/sys/LuaSysApi.hpp>
#include <sihd/lua/util/LuaUtilApi.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/term.hpp>

#include "../lua_fixture.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::lua;
class TestLuaSysApi: public test::LuaFixture
{
};

TEST_F(TestLuaSysApi, test_luasys_uuid_procinfo)
{
    LuaUtilApi::load_base(_vm);
    LuaSysApi::load_base(_vm);
    LuaSysApi::load_tools(_vm);
    this->do_script("test/sys/lua/test_uuid_procinfo.lua");
}

TEST_F(TestLuaSysApi, test_luasys_bitmap)
{
    LuaUtilApi::load_base(_vm);
    LuaSysApi::load_base(_vm);
    this->do_script("test/sys/lua/test_bitmap.lua");
}

TEST_F(TestLuaSysApi, test_luasys_tools)
{
    LuaUtilApi::load_base(_vm);
    LuaSysApi::load_base(_vm);
    LuaSysApi::load_tools(_vm);
    this->do_script("test/sys/lua/test_sys_tools.lua");
}

TEST_F(TestLuaSysApi, test_luasys_process)
{
    LuaUtilApi::load_base(_vm);
    LuaSysApi::load_base(_vm);
    LuaSysApi::load_process(_vm);
    this->do_script("test/sys/lua/test_process.lua");
}

TEST_F(TestLuaSysApi, test_luasys_process_errors)
{
    LuaUtilApi::load_base(_vm);
    LuaSysApi::load_base(_vm);
    LuaSysApi::load_process(_vm);
    this->do_script("test/sys/lua/test_process_errors.lua");
}

TEST_F(TestLuaSysApi, test_luasys_files)
{
    LuaUtilApi::load_base(_vm);
    LuaSysApi::load_base(_vm);
    LuaSysApi::load_files(_vm);
    this->do_script("test/sys/lua/test_files.lua");
}

TEST_F(TestLuaSysApi, test_luasys_proc)
{
    LuaUtilApi::load_base(_vm);
    LuaUtilApi::load_tools(_vm);
    LuaSysApi::load_base(_vm);
    LuaSysApi::load_process(_vm);
    this->do_script("test/sys/lua/test_proc.lua");
}

} // namespace test

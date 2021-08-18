#include <sihd/lua/LuaUtilApi.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::lua
{

NEW_LOGGER("sihd::lua");

using namespace sihd::util;

LuaUtilApi::LuaUtilApi()
{   
}

LuaUtilApi::~LuaUtilApi()
{
}

void    LuaUtilApi::load(sol::state & lua)
{
    lua.new_usertype<Named>("Named",
        sol::constructors<Named(const std::string &, Node *)>(),
        "get_parent", &Named::get_parent,
        "get_name", &Named::get_name);

    lua.new_usertype<Node>("Node",
        sol::constructors<Node(const std::string &, Node *)>(),
        "get_child", &Node::get_child<Named>,
        "print_tree", static_cast<void (Node::*)()>(&Node::print_tree),
        sol::base_classes, sol::bases<Named>());
}

}

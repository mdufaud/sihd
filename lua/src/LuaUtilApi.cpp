#include <sihd/lua/LuaUtilApi.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/AtExit.hpp>
#include <sihd/util/Task.hpp>
#include <sihd/util/SmartNodePtr.hpp>

namespace sihd::lua
{

NEW_LOGGER("sihd::lua");

using namespace sihd::util;

Node *LuaUtilApi::root = nullptr;

void    LuaUtilApi::load(sol::state & lua)
{
    lua.new_usertype<Named>("Named",
        sol::factories(
            [] (const std::string & name, Node *parent) { return SmartNodePtr<Named>(new Named(name, parent)); },
            [] (const std::string & name) { return SmartNodePtr<Named>(new Named(name)); }
        ),
        "get_parent", &Named::get_parent,
        "get_name", &Named::get_name);

    lua.new_usertype<Node>("Node",
        sol::factories(
            [] (const std::string & name, Node *parent) { return SmartNodePtr<Node>(new Node(name, parent)); },
            [] (const std::string & name) { return SmartNodePtr<Node>(new Node(name)); }
        ),
        "get_child", &Node::get_child<Named>,
        "print_tree", static_cast<void (Node::*)()>(&Node::print_tree),
        sol::base_classes, sol::bases<Named>());
}

}

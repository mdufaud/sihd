#include <sihd/lua/LuaUtilApi.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::lua
{

NEW_LOGGER("sihd::lua");

using namespace sihd::util;

Node *LuaUtilApi::root = nullptr;

LuaUtilApi::LuaUtilApi()
{   
}

LuaUtilApi::~LuaUtilApi()
{
}

void    LuaUtilApi::unload()
{
    if (root != nullptr)
    {
        delete root;
        root = nullptr;
    }
}

void    LuaUtilApi::load(sol::state & lua)
{
    Node *sihd = new Node("sihd");

    root = sihd;

    lua.set("sihd", sihd);

    lua.new_usertype<Named>("Named",
        sol::factories([sihd] (const std::string & name, Node *parent) { return new Named(name, parent == nullptr ? sihd : parent); }),
        //sol::constructors<Named(const std::string &, Node *)>(),
        //"__gc", sol::destructor([&] (Named *n) { TRACE(n->get_name()); } ),
        "get_parent", &Named::get_parent,
        "get_name", &Named::get_name);

    lua.new_usertype<Node>("Node",
        sol::factories([sihd] (const std::string & name, Node *parent) { return new Node(name, parent == nullptr ? sihd : parent); }),
        //sol::constructors<Node(const std::string &, Node *)>(),
        "get_child", &Node::get_child<Named>,
        "print_tree", static_cast<void (Node::*)()>(&Node::print_tree),
        sol::base_classes, sol::bases<Named>());
}

}

#include <sihd/lua/LuaUtilApi.hpp>
#include <sihd/util/AtExit.hpp>
#include <sihd/util/Named.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/SmartNodePtr.hpp>
#include <sihd/lua/LuaApi.hpp>

namespace sihd::lua
{

sihd::util::Logger LuaUtilApi::logger("sihd::lua");

using namespace sihd::util;

void    LuaUtilApi::load(sol::state & lua)
{
    LuaApi::load(lua);
    sol::table sihd = lua["sihd"];
    if (sihd.get_or("util", nullptr) != nullptr)
        return ;
    sol::table sihd_util = lua.create_table();
    // log
    sol::table sihd_log = lua.create_table();
    sihd_log.set_function("debug", [] (const std::string & log) { logger.log(debug, log.c_str()); });
    sihd_log.set_function("info", [] (const std::string & log) { logger.log(info, log.c_str()); });
    sihd_log.set_function("warning", [] (const std::string & log) { logger.log(warning, log.c_str()); });
    sihd_log.set_function("error", [] (const std::string & log) { logger.log(error, log.c_str()); });
    sihd_log.set_function("critical", [] (const std::string & log) { logger.log(critical, log.c_str()); });
    sihd_util["log"] = sihd_log;
    // named
    sihd_util.new_usertype<Named>("Named",
        sol::factories(
            [] (const std::string & name, Node *parent) { return SmartNodePtr<Named>(new Named(name, parent)); },
            [] (const std::string & name) { return SmartNodePtr<Named>(new Named(name)); }
        ),
        "get_parent", &Named::get_parent,
        "get_name", &Named::get_name);
    // node
    sihd_util.new_usertype<Node>("Node",
        sol::factories(
            [] (const std::string & name, Node *parent) { return SmartNodePtr<Node>(new Node(name, parent)); },
            [] (const std::string & name) { return SmartNodePtr<Node>(new Node(name)); }
        ),
        "get_child", &Node::get_child<Named>,
        "print_tree", static_cast<void (Node::*)() const>(&Node::print_tree),
        sol::base_classes, sol::bases<Named>());
    sihd["util"] = sihd_util;
}

}

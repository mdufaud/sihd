#include <sihd/lua/LuaUtilApi.hpp>
#include <sihd/lua/LuaApi.hpp>

#include <sihd/util/SmartNodePtr.hpp>
#include <sihd/util/Named.hpp>
#include <sihd/util/Node.hpp>

#include <sihd/util/time.hpp>
#include <sihd/util/Clocks.hpp>
#include <sihd/util/Thread.hpp>

#include <sihd/util/Types.hpp>
#include <sihd/util/Array.hpp>

#include <sihd/util/IWriter.hpp>

#include <sihd/util/Observable.hpp>
#include <sihd/util/ObserverWaiter.hpp>

#include <sihd/util/Process.hpp>

#define DECLARE_ARRAY_USERTYPE(ArrType, PrimitiveType) sihd_util.new_usertype<ArrType>(#ArrType, \
        sol::constructors<ArrType(), ArrType(size_t)>(), \
        sol::call_constructor, sol::factories( \
            [] (sol::table tbl) \
            { \
                ArrType ret(tbl.size()); \
                for (const auto & key_pair: tbl) \
                    ret.push_back(key_pair.second.as<PrimitiveType>()); \
                return ret; \
            } \
        ), \
        "clone", &ArrType::clone, \
        "push_back", static_cast<bool (ArrType::*)(const PrimitiveType &)>(&ArrType::push_back), \
        "pop", &ArrType::pop, \
        "front", &ArrType::front, \
        "back", &ArrType::back, \
        "at", &ArrType::at, \
        "set", &ArrType::set, \
        sol::base_classes, sol::bases<IArray>());

namespace sihd::lua
{

sihd::util::Logger LuaUtilApi::logger("sihd::lua");

using namespace sihd::util;

void    LuaUtilApi::add_named(sol::state & lua, sol::table & sihd_util)
{
    (void)lua;
    sihd_util.new_usertype<Named>("Named",
        sol::factories(
            [] (const std::string & name, Node *parent) { return SmartNodePtr<Named>(new Named(name, parent)); },
            [] (const std::string & name) { return SmartNodePtr<Named>(new Named(name)); }
        ),
        "get_parent", &Named::get_parent,
        "get_name", &Named::get_name,
        "get_full_name", &Named::get_full_name,
        "get_class_name", &Named::get_class_name,
        "get_description", &Named::get_description,
        "get_root", &Named::get_root,
        "find", static_cast<Named * (Named::*)(const std::string &)>(&Named::find),
        "find_from", static_cast<Named * (Named::*)(Named *, const std::string &)>(&Named::find));
}

void    LuaUtilApi::add_node(sol::state & lua, sol::table & sihd_util)
{
    (void)lua;
    sihd_util.new_usertype<Node::ChildEntry>("ChildEntry",
        sol::no_constructor,
        "name", &Node::ChildEntry::name,
        "obj", &Node::ChildEntry::obj,
        "ownership", &Node::ChildEntry::ownership);
    sihd_util.new_usertype<Node>("Node",
        sol::factories(
            [] (const std::string & name, Node *parent) { return SmartNodePtr<Node>(new Node(name, parent)); },
            [] (const std::string & name) { return SmartNodePtr<Node>(new Node(name)); }
        ),
        "get_child", &Node::get_child<Named>,
        "add_child", [] (Node & self, Named *child) { return self.add_child(child); },
        "add_child_name", [] (Node & self, const std::string & name, Named *child) { return self.add_child(name, child); },
        "delete_child", static_cast<bool (Node::*)(const Named *)>(&Node::delete_child),
        "delete_child_name", static_cast<bool (Node::*)(const std::string &)>(&Node::delete_child),
        "get_child", static_cast<Named * (Node::*)(const std::string &) const>(&Node::get_child),
        "is_link", &Node::is_link,
        "add_link", &Node::add_link,
        "remove_link", &Node::remove_link,
        "get_link", &Node::get_link,
        "resolve_links", &Node::resolve_links,
        "get_tree_str", static_cast<std::string (Node::*)() const>(&Node::get_tree_str),
        "get_tree_desc_str", &Node::get_tree_desc_str,
        "get_children", &Node::get_children,
        "get_children_keys", &Node::get_children_keys,
        sol::base_classes, sol::bases<Named>());
}

void    LuaUtilApi::add_log(sol::state & lua, sol::table & sihd_util)
{
    sol::table sihd_log = lua.create_table();
    sihd_log.set_function("debug", [] (const std::string & log) { logger.log(debug, log.c_str()); });
    sihd_log.set_function("info", [] (const std::string & log) { logger.log(info, log.c_str()); });
    sihd_log.set_function("warning", [] (const std::string & log) { logger.log(warning, log.c_str()); });
    sihd_log.set_function("error", [] (const std::string & log) { logger.log(error, log.c_str()); });
    sihd_log.set_function("critical", [] (const std::string & log) { logger.log(critical, log.c_str()); });
    sihd_util["log"] = sihd_log;
}

void    LuaUtilApi::add_clock(sol::state & lua, sol::table & sihd_util)
{
    (void)lua;
    sihd_util.new_usertype<IClock>("IClock",
        sol::no_constructor,
        "now", &IClock::now,
        "is_steady", &IClock::is_steady);
    sihd_util.new_usertype<SystemClock>("SystemClock",
        sol::constructors<SystemClock()>(),
        sol::base_classes, sol::bases<IClock>());
    sihd_util.new_usertype<SteadyClock>("SteadyClock",
        sol::constructors<SteadyClock()>(),
        sol::base_classes, sol::bases<IClock>());
    sihd_util["clock"] = Clock::default_clock;
}

void    LuaUtilApi::add_str(sol::state & lua, sol::table & sihd_util)
{
    (void)lua;
    (void)sihd_util;
}

void    LuaUtilApi::add_time(sol::state & lua, sol::table & sihd_util)
{
    sol::table sihd_time = lua.create_table();

    sihd_time.set_function("sleep", &time::sleep);
    sihd_time.set_function("nsleep", &time::nsleep);
    sihd_time.set_function("usleep", &time::usleep);
    sihd_time.set_function("msleep", &time::msleep);

    sihd_time.set_function("to_us", &time::to_micro);
    sihd_time.set_function("to_ms", &time::to_milli);
    sihd_time.set_function("to_sec", &time::to_sec);
    sihd_time.set_function("to_min", &time::to_min);
    sihd_time.set_function("to_hours", &time::to_hours);
    sihd_time.set_function("to_days", &time::to_days);
    sihd_time.set_function("to_double", &time::to_double);
    sihd_time.set_function("to_hz", &time::to_freq);

    sihd_time.set_function("us", &time::micro);
    sihd_time.set_function("ms", &time::milli);
    sihd_time.set_function("sec", &time::sec);
    sihd_time.set_function("min", &time::min);
    sihd_time.set_function("hours", &time::hours);
    sihd_time.set_function("days", &time::days);
    sihd_time.set_function("hz", &time::freq);
    sihd_time.set_function("from_double", &time::from_double);

    sihd_util["time"] = sihd_time;
}

void    LuaUtilApi::add_thread(sol::state & lua, sol::table & sihd_util)
{
    sol::table sihd_thread = lua.create_table();
    sihd_thread.set_function("id", &Thread::id);
    sihd_thread.set_function("id_str", static_cast<std::string (*)()>(&Thread::id_str));
    sihd_thread.set_function("set_name", &Thread::set_name);
    sihd_thread.set_function("get_name", &Thread::get_name);
    sihd_util["thread"] = sihd_thread;
}

void    LuaUtilApi::add_types(sol::state & lua, sol::table & sihd_util)
{
    sol::table sihd_types = lua.create_table();
    sihd_types.set_function("type_size", &Types::type_size);
    sihd_types.set_function("type_to_string", &Types::type_to_string);
    sihd_types.set_function("string_to_type", &Types::string_to_type);
    sihd_util["types"] = sihd_types;
}

void    LuaUtilApi::add_array(sol::state & lua, sol::table & sihd_util)
{
    sihd_util.new_usertype<IArray>("IArray",
        sol::no_constructor,
        "size", &IArray::size,
        "capacity", &IArray::capacity,
        "data_size", &IArray::data_size,
        "byte_size", &IArray::byte_size,
        "resize", &IArray::resize,
        "reserve", &IArray::reserve,
        "byte_resize", &IArray::byte_resize,
        "byte_reserve", &IArray::byte_reserve,
        "data_type", &IArray::data_type,
        "data_type_to_string", &IArray::data_type_to_string,
        "hexdump", &IArray::hexdump,
        "to_string", &IArray::to_string,
        "clear", &IArray::clear);

    DECLARE_ARRAY_USERTYPE(ArrBool, bool);
    DECLARE_ARRAY_USERTYPE(ArrChar, char);
    DECLARE_ARRAY_USERTYPE(ArrByte, int8_t);
    DECLARE_ARRAY_USERTYPE(ArrUByte, uint8_t);
    DECLARE_ARRAY_USERTYPE(ArrShort, int16_t);
    DECLARE_ARRAY_USERTYPE(ArrUShort, uint16_t);
    DECLARE_ARRAY_USERTYPE(ArrInt, int32_t);
    DECLARE_ARRAY_USERTYPE(ArrUInt, uint32_t);
    DECLARE_ARRAY_USERTYPE(ArrLong, int64_t);
    DECLARE_ARRAY_USERTYPE(ArrULong, uint64_t);
    DECLARE_ARRAY_USERTYPE(ArrFloat, float);
    DECLARE_ARRAY_USERTYPE(ArrDouble, double);

    // a bit of a specialization for ArrStr
    sihd_util.new_usertype<ArrStr>("ArrStr",
        sol::constructors<ArrStr(), ArrStr(size_t), ArrStr(const std::string &)>(),
        "clone", &ArrStr::clone,
        "push_back", static_cast<bool (ArrStr::*)(const std::string &)>(&ArrStr::push_back),
        "pop", &ArrStr::pop,
        "front", &ArrStr::front,
        "back", &ArrStr::back,
        "at", &ArrStr::at,
        "set", &ArrStr::set,
        "str", &ArrStr::cpp_str,
        sol::base_classes, sol::bases<IArray>());

    sol::table sihd_array_util = lua.create_table();
    sihd_array_util.set_function("create_from_type", &ArrayUtil::create_from_type);
    sihd_util["ArrayUtils"] = sihd_array_util;
}

bool    LuaUtilApi::_configurable_set(Configurable & obj, const std::string & key, sol::object value)
{
    switch (value.get_type())
    {
        case sol::type::boolean:
        {
            return obj.set_conf<bool>(key, value.as<bool>());
        }
        case sol::type::table:
        {
            bool ret = true;
            for (const auto & key_pair: value.as<sol::table>())
                ret = LuaUtilApi::_configurable_set(obj, key, key_pair.second) && ret;
            return ret;
        }
        case sol::type::number:
        {
            if (value.is<int>())
                return obj.set_conf_int(key, value.as<int>());
            else if (value.is<double>())
                return obj.set_conf_float(key, value.as<double>());
            return false;
        }
        case sol::type::string:
        {
            return obj.set_conf_str(key, value.as<std::string>());
        }
        default:
        {
            logger.error(Str::format("Configuration '%s' type error", key.c_str()).c_str());
        }
    }
    return false;
}

void    LuaUtilApi::add_configurable(sol::state & lua, sol::table & sihd_util)
{
    (void)lua;
    sihd_util.new_usertype<Configurable>("Configurable",
        sol::no_constructor,
        "set_conf", [] (Configurable & self, sol::table tbl) -> bool
        {
            bool ret = true;
            for (const auto & key_pair: tbl)
            {
                sol::object key_obj = key_pair.first;
                if (key_obj.get_type() != sol::type::string)
                {
                    logger.error("Configuration key is not a string");
                    continue ;
                }
                std::string key = key_obj.as<std::string>();
                sol::object value = key_pair.second;
                ret = LuaUtilApi::_configurable_set(self, key, key_pair.second) && ret;
            }
            return ret;
        });
}

void    LuaUtilApi::add_interfaces(sol::state & lua, sol::table & sihd_util)
{
    (void)lua;
    sihd_util.new_usertype<IRunnable>("IRunnable",
        sol::no_constructor,
        "run", &IRunnable::run);
    sihd_util.new_usertype<ISteppable>("ISteppable",
        sol::no_constructor,
        "step", &ISteppable::step);
    sihd_util.new_usertype<IStoppableRunnable>("IStoppableRunnable",
        sol::no_constructor,
        "stop", &IStoppableRunnable::stop,
        "is_running", &IStoppableRunnable::is_running,
        sol::base_classes, sol::bases<IRunnable>());
    sihd_util.new_usertype<IWriter>("IWriter",
        sol::no_constructor,
        "write", &IWriter::write);
    sihd_util.new_usertype<IWriterTimestamp>("IWriterTimestamp",
        sol::no_constructor,
        "write", static_cast<ssize_t (IWriterTimestamp::*)(const char *, size_t)>(&IWriter::write),
        "write", static_cast<ssize_t (IWriterTimestamp::*)(const char *, size_t, time_t)>(&IWriterTimestamp::write));
}

void    LuaUtilApi::load(sol::state & lua)
{
    LuaApi::load(lua);
    sol::table sihd = lua["sihd"];
    if (sihd.get_or("util", nullptr) != nullptr)
        return ;
    sol::table sihd_util = lua.create_table();
    LuaUtilApi::add_named(lua, sihd_util);
    LuaUtilApi::add_node(lua, sihd_util);
    LuaUtilApi::add_log(lua, sihd_util);
    LuaUtilApi::add_clock(lua, sihd_util);
    LuaUtilApi::add_time(lua, sihd_util);
    LuaUtilApi::add_thread(lua, sihd_util);
    LuaUtilApi::add_str(lua, sihd_util);
    LuaUtilApi::add_types(lua, sihd_util);
    LuaUtilApi::add_array(lua, sihd_util);
    LuaUtilApi::add_configurable(lua, sihd_util);
    LuaUtilApi::add_interfaces(lua, sihd_util);
    sihd["util"] = sihd_util;
}

/* ************************************************************************* */
/* */
/* ************************************************************************* */

}

#ifndef __SIHD_LUA_LUACOREAPI_HPP__
# define __SIHD_LUA_LUACOREAPI_HPP__

# include <sol/sol.hpp>
# include <sihd/util/Logger.hpp>

#include <sihd/util/Configurable.hpp>

# include <sihd/util/IRunnable.hpp>
# include <sihd/util/ISteppable.hpp>
# include <sihd/util/IStoppableRunnable.hpp>

namespace sihd::lua
{

class LuaUtilApi
{
    public:
        static void load(sol::state & state);

        static sihd::util::Logger logger;

    private:
        LuaUtilApi() {};
        virtual ~LuaUtilApi() {};

        static void add_named(sol::state & lua, sol::table & sihd_util);
        static void add_node(sol::state & lua, sol::table & sihd_util);

        static void add_log(sol::state & lua, sol::table & sihd_util);
        static void add_clock(sol::state & lua, sol::table & sihd_util);
        static void add_str(sol::state & lua, sol::table & sihd_util);
        static void add_time(sol::state & lua, sol::table & sihd_util);
        static void add_thread(sol::state & lua, sol::table & sihd_util);

        static void add_types(sol::state & lua, sol::table & sihd_util);
        static void add_array(sol::state & lua, sol::table & sihd_util);

        static void add_configurable(sol::state & lua, sol::table & sihd_util);
        static void add_interfaces(sol::state & lua, sol::table & sihd_util);

        //
        static void add_process(sol::state & lua, sol::table & sihd_util);
        static void add_service(sol::state & lua, sol::table & sihd_util);
        static void add_scheduler(sol::state & lua, sol::table & sihd_util);
        static void add_file(sol::state & lua, sol::table & sihd_util);
        static void add_os(sol::state & lua, sol::table & sihd_util);
        static void add_fs(sol::state & lua, sol::table & sihd_util);
        static void add_endian(sol::state & lua, sol::table & sihd_util);
        static void add_daemon(sol::state & lua, sol::table & sihd_util);
        static void add_path(sol::state & lua, sol::table & sihd_util);
        static void add_shared_memory(sol::state & lua, sol::table & sihd_util);

        class LuaRunnable: public sihd::util::IRunnable
        {
            LuaRunnable(sol::function fun);
            ~LuaRunnable();

            bool run();

            sol::function _fun;
        };

        class LuaSteppable: public sihd::util::ISteppable
        {
            LuaSteppable(sol::table tbl);
            ~LuaSteppable();

            bool step();

            sol::function _fun;
        };

        class LuaStoppableRunnable: public sihd::util::IStoppableRunnable
        {
            LuaStoppableRunnable(sol::table tbl);
            ~LuaStoppableRunnable();

            bool run();
            bool stop();
            bool is_running() const;

            sol::function _run;
            sol::function _stop;
            sol::function _is_running;
        };

        static bool _configurable_set(sihd::util::Configurable & obj, const std::string & key, sol::object value);

};

}

#endif

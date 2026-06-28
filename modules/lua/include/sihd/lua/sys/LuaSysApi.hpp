#ifndef __SIHD_LUA_LUASYSAPI_HPP__
#define __SIHD_LUA_LUASYSAPI_HPP__

#include <sihd/lua/Vm.hpp>
#include <sihd/lua/util/LuaUtilApi.hpp>

#include <sihd/sys/Process.hpp>

#include <memory>
#include <mutex>
#include <queue>

namespace sihd::lua
{

class LuaSysApi
{
    public:
        static void load_all(Vm & vm);

        static void load_base(Vm & vm);
        static void load_tools(Vm & vm);
        static void load_files(Vm & vm);
        static void load_process(Vm & vm);

        /**
         * Thread-safe callback for Process stdout/stderr
         * Data is queued and must be flushed from the Lua thread via flush()
         */
        class LuaProcessCallback
        {
            public:
                LuaProcessCallback(luabridge::LuaRef callback);
                ~LuaProcessCallback();

                // Called from Process reader thread - thread-safe
                void operator()(std::string_view data);

                // Called from Lua thread to process queued data
                void flush();

                // Check if there's pending data
                bool has_pending() const;

            private:
                luabridge::LuaRef _callback;
                std::queue<std::string> _queue;
                mutable std::mutex _mutex;
        };

    private:
        LuaSysApi() = delete;
        ~LuaSysApi() = delete;
};

} // namespace sihd::lua

#endif

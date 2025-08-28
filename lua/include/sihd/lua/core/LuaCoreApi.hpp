#ifndef __SIHD_LUA_LUACOREAPI_HPP__
#define __SIHD_LUA_LUACOREAPI_HPP__

#include <sihd/lua/Vm.hpp>
#include <sihd/lua/util/LuaUtilApi.hpp>

#include <sihd/util/Runnable.hpp>

#include <sihd/core/Channel.hpp>

#include <queue>

namespace sihd::lua
{

class LuaCoreApi
{
    public:
        static void load(Vm & vm);

        class LuaChannelHandler: public sihd::util::IHandler<sihd::core::Channel *>
        {
            public:
                LuaChannelHandler();
                ~LuaChannelHandler();

                void handle(sihd::core::Channel *c) override;

                void add_channel_obs(sihd::core::Channel *c, lua_State *state, luabridge::LuaRef ref);
                void remove_channel_obs(sihd::core::Channel *c);

            protected:
                bool handle_channel_thread();

            private:
                struct LuaSingleChannelHandler
                {
                        LuaSingleChannelHandler(lua_State *state, luabridge::LuaRef ref):
                            state(state),
                            thread_runner(ref)
                        {
                        }

                        lua_State *state;
                        LuaUtilApi::LuaThreadRunner thread_runner;
                };

                std::map<sihd::core::Channel *, LuaSingleChannelHandler> _map_handler;
                std::mutex _mutex_map;
        };

    protected:

    private:
        LuaCoreApi() = default;
        virtual ~LuaCoreApi() = default;

        static LuaChannelHandler _channel_handler;
};

} // namespace sihd::lua

#endif
#ifndef __SIHD_LUA_LUACOREAPI_HPP__
# define __SIHD_LUA_LUACOREAPI_HPP__

# include <sihd/lua/Vm.hpp>
# include <sihd/lua/LuaUtilApi.hpp>

# include <sihd/core/Channel.hpp>

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

                void handle(sihd::core::Channel *c);

                void add_channel_obs(sihd::core::Channel *c, luabridge::LuaRef ref);
                void remove_channel_obs(sihd::core::Channel *c);

            private:
                std::map<sihd::core::Channel *, luabridge::LuaRef> _map_handler;
        };

    protected:

    private:
        LuaCoreApi() {}
        virtual ~LuaCoreApi() {}

        static LuaChannelHandler _channel_handler;

};

}

#endif
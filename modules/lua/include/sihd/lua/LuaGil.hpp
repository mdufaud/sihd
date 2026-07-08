#ifndef __SIHD_LUA_LUAGIL_HPP__
#define __SIHD_LUA_LUAGIL_HPP__

#include <condition_variable>
#include <mutex>
#include <thread>

struct lua_State;

namespace sihd::lua
{

// Per-universe Lua execution lock (GIL), reentrant per-thread. Lua built without lua_lock.
class LuaGil
{
    public:
        LuaGil() = default;
        ~LuaGil() = default;

        LuaGil(const LuaGil &) = delete;
        LuaGil & operator=(const LuaGil &) = delete;

        void lock();
        void unlock();

        // release the whole lock around a blocking call, returns the saved depth (0 if not held)
        int release_save();
        void acquire_restore(int depth);

    private:
        std::mutex _mutex;
        std::condition_variable _cv;
        std::thread::id _owner;
        int _depth = 0;
};

// RAII hold of the universe GIL during a Lua API region; no-op if the state has no GIL.
class LuaGilGuard
{
    public:
        explicit LuaGilGuard(lua_State *state);
        ~LuaGilGuard();

        LuaGilGuard(const LuaGilGuard &) = delete;
        LuaGilGuard & operator=(const LuaGilGuard &) = delete;

    private:
        LuaGil *_gil;
};

// RAII release of the universe GIL around a blocking C++ call so other threads can run Lua.
class LuaGilRelease
{
    public:
        explicit LuaGilRelease(lua_State *state);
        ~LuaGilRelease();

        LuaGilRelease(const LuaGilRelease &) = delete;
        LuaGilRelease & operator=(const LuaGilRelease &) = delete;

    private:
        LuaGil *_gil;
        int _depth;
};

} // namespace sihd::lua

#endif

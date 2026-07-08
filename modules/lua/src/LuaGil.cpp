#include <sihd/lua/LuaGil.hpp>
#include <sihd/lua/Vm.hpp>

namespace sihd::lua
{

void LuaGil::lock()
{
    const auto self = std::this_thread::get_id();
    std::unique_lock l(_mutex);
    if (_depth > 0 && _owner == self)
    {
        ++_depth;
        return;
    }
    _cv.wait(l, [this] { return _depth == 0; });
    _owner = self;
    _depth = 1;
}

void LuaGil::unlock()
{
    std::lock_guard l(_mutex);
    if (_depth == 0 || _owner != std::this_thread::get_id())
        return;
    if (--_depth == 0)
    {
        _owner = std::thread::id();
        _cv.notify_one();
    }
}

int LuaGil::release_save()
{
    std::lock_guard l(_mutex);
    if (_depth == 0 || _owner != std::this_thread::get_id())
        return 0;
    const int depth = _depth;
    _depth = 0;
    _owner = std::thread::id();
    _cv.notify_one();
    return depth;
}

void LuaGil::acquire_restore(int depth)
{
    if (depth == 0)
        return;
    std::unique_lock l(_mutex);
    _cv.wait(l, [this] { return _depth == 0; });
    _owner = std::this_thread::get_id();
    _depth = depth;
}

/* ************************************************************************* */
/* LuaGilGuard */
/* ************************************************************************* */

LuaGilGuard::LuaGilGuard(lua_State *state): _gil(Vm::get_gil(state))
{
    if (_gil != nullptr)
        _gil->lock();
}

LuaGilGuard::~LuaGilGuard()
{
    if (_gil != nullptr)
        _gil->unlock();
}

/* ************************************************************************* */
/* LuaGilRelease */
/* ************************************************************************* */

LuaGilRelease::LuaGilRelease(lua_State *state): _gil(Vm::get_gil(state)), _depth(0)
{
    if (_gil != nullptr)
        _depth = _gil->release_save();
}

LuaGilRelease::~LuaGilRelease()
{
    if (_gil != nullptr)
        _gil->acquire_restore(_depth);
}

} // namespace sihd::lua

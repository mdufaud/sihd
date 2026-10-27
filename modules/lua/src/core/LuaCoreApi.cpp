#include <sihd/core/ACoreObject.hpp>
#include <sihd/core/Channel.hpp>
#include <sihd/core/ChannelWaiter.hpp>
#include <sihd/core/Core.hpp>
#include <sihd/core/DevBatcher.hpp>
#include <sihd/core/DevFilter.hpp>
#include <sihd/core/DevMessage.hpp>
#include <sihd/core/DevPlayer.hpp>
#include <sihd/core/DevPulsation.hpp>
#include <sihd/core/DevRecorder.hpp>
#include <sihd/core/DevSampler.hpp>
#include <sihd/core/DevStats.hpp>
#include <sihd/core/DevWorker.hpp>
#include <sihd/core/DevWorkerPool.hpp>
#include <sihd/core/Device.hpp>
#include <sihd/lua/checked.hpp>
#include <sihd/lua/core/LuaCoreApi.hpp>
#include <sihd/lua/util/LuaUtilApi.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/SmartNodePtr.hpp>

namespace sihd::lua
{

SIHD_LOGGER;

using namespace sihd::util;
using namespace sihd::core;

namespace
{

std::mutex handler_map_mutex;
std::map<lua_State *, std::unique_ptr<LuaCoreApi::LuaChannelHandler>> handler_map;

} // namespace

LuaCoreApi::LuaChannelHandler *LuaCoreApi::get_handler(lua_State *state)
{
    std::lock_guard lock(handler_map_mutex);
    auto it = handler_map.find(state);
    if (it != handler_map.end())
        return it->second.get();
    return nullptr;
}

void LuaCoreApi::unload(Vm & vm)
{
    std::lock_guard lock(handler_map_mutex);
    handler_map.erase(vm.lua_state());
}

void LuaCoreApi::load(Vm & vm)
{
    {
        std::lock_guard lock(handler_map_mutex);
        handler_map[vm.lua_state()] = std::make_unique<LuaChannelHandler>(vm.lua_state());
    }

    luabridge::getGlobalNamespace(vm.lua_state())
        .beginNamespace("sihd")
        .beginNamespace("core")
        .deriveClass<Device, Node>("Device")
        .addFunction("set_conf", checked<&LuaUtilApi::configurable_set_conf<Device>>)
        .addFunction("find_channel", checked<+[](Device *self, const std::string & name) {
                         return self->find_channel(name);
                     }>)
        .addFunction("get_channel", checked<+[](Device *self, const std::string & name) {
                         return self->get_channel(name);
                     }>)
        .addFunction(
            "add_channel",
            static_cast<Channel *(Device::*)(const std::string &, std::string_view, size_t)>(&Device::add_channel))
        .addFunction("setup", static_cast<bool (Device::*)()>(&ACoreService::setup))
        .addFunction("init", static_cast<bool (Device::*)()>(&ACoreService::init))
        .addFunction("start", static_cast<bool (Device::*)()>(&ACoreService::start))
        .addFunction("stop", static_cast<bool (Device::*)()>(&ACoreService::stop))
        .addFunction("reset", static_cast<bool (Device::*)()>(&ACoreService::reset))
        .addFunction("is_running", static_cast<bool (Device::*)() const>(&ACoreService::is_running))
        .addFunction("device_state", &Device::device_state)
        .addFunction("device_state_str", &Device::device_state_str)
        .addFunction(
            "service_ctrl",
            +[](Device *self) -> ServiceController * {
                // service_ctrl() is public on AService (protected override on Device)
                return dynamic_cast<ServiceController *>(static_cast<AService *>(self)->service_ctrl());
            })
        .endClass()
        .deriveClass<Core, Device>("Core")
        .addConstructorFrom<SmartNodePtr<Core>, void(const std::string &, Node *)>()
        .endClass()
        .deriveClass<Channel, Named>("Channel")
        .addConstructorFrom<SmartNodePtr<Channel>, void(const std::string &, const std::string &, size_t, Node *)>()
        .addFunction("set_write_on_change", &Channel::set_write_on_change)
        .addFunction("notify", &Channel::notify)
        .addFunction("timestamp", &Channel::timestamp)
        .addFunction("size", &Channel::size)
        .addFunction("capacity", &Channel::capacity)
        .addFunction("reserve", &Channel::reserve)
        .addFunction("resize", &Channel::resize)
        .addFunction("array", static_cast<const IArray *(Channel::*)() const>(&Channel::array))
        .addFunction(
            "set_observer",
            checked<+[](Channel *self, luabridge::LuaRef fun, lua_State *state) -> Result<void> {
                LuaChannelHandler *handler = LuaCoreApi::get_handler(state);
                if (handler == nullptr)
                    return std::unexpected(Error(ErrorCode::not_initialized, "LuaCoreApi not loaded for this VM"));
                if (fun.isNil())
                {
                    handler->remove_channel_obs(self);
                    LuaGilRelease release(state);
                    self->remove_observer(handler);
                }
                else if (fun.isFunction())
                {
                    handler->add_channel_obs(self, state, fun);
                    LuaGilRelease release(state);
                    self->add_observer(handler);
                }
                else
                    return std::unexpected(
                        Error(ErrorCode::invalid_argument, "set_observer argument must be a function"));
                return {};
            }>)
        .addFunction(
            "copy_to",
            +[](Channel *self, IArray *array_ptr) { return self->copy_to(*array_ptr); })
        .addFunction(
            "write_array",
            +[](Channel *self, const IArray *array_ptr) {
                return array_ptr != nullptr ? self->write(*array_ptr) : false;
            })
        .addFunction(
            "write",
            +[](Channel *self, size_t idx, luabridge::LuaRef arg) {
                if (self->array() == nullptr)
                    return false;
                switch (self->array()->data_type())
                {
                    case TYPE_BOOL:
                        return self->write<bool>(idx, static_cast<bool>(arg));
                    case TYPE_CHAR:
                        return self->write<char>(idx, static_cast<char>(arg));
                    case TYPE_BYTE:
                        return self->write<int8_t>(idx, static_cast<int8_t>(arg));
                    case TYPE_UBYTE:
                        return self->write<uint8_t>(idx, static_cast<uint8_t>(arg));
                    case TYPE_SHORT:
                        return self->write<int16_t>(idx, static_cast<int16_t>(arg));
                    case TYPE_USHORT:
                        return self->write<uint16_t>(idx, static_cast<uint16_t>(arg));
                    case TYPE_INT:
                        return self->write<int32_t>(idx, static_cast<int32_t>(arg));
                    case TYPE_UINT:
                        return self->write<uint32_t>(idx, static_cast<uint32_t>(arg));
                    case TYPE_LONG:
                        return self->write<int64_t>(idx, static_cast<int64_t>(arg));
                    case TYPE_ULONG:
                        return self->write<uint64_t>(idx, static_cast<uint64_t>(arg));
                    case TYPE_FLOAT:
                        return self->write<float>(idx, static_cast<float>(arg));
                    case TYPE_DOUBLE:
                        return self->write<double>(idx, static_cast<double>(arg));
                    default:
                        return false;
                }
            })
        .addFunction(
            "read",
            +[](Channel *self, size_t idx, lua_State *state) {
                luabridge::LuaRef ret(state);
                if (self->array() == nullptr)
                    return ret;
                switch (self->array()->data_type())
                {
                    case TYPE_BOOL:
                        ret = self->read<bool>(idx);
                        break;
                    case TYPE_CHAR:
                        ret = self->read<char>(idx);
                        break;
                    case TYPE_BYTE:
                        ret = self->read<int8_t>(idx);
                        break;
                    case TYPE_UBYTE:
                        ret = self->read<uint8_t>(idx);
                        break;
                    case TYPE_SHORT:
                        ret = self->read<int16_t>(idx);
                        break;
                    case TYPE_USHORT:
                        ret = self->read<uint16_t>(idx);
                        break;
                    case TYPE_INT:
                        ret = self->read<int32_t>(idx);
                        break;
                    case TYPE_UINT:
                        ret = self->read<uint32_t>(idx);
                        break;
                    case TYPE_LONG:
                        ret = self->read<int64_t>(idx);
                        break;
                    case TYPE_ULONG:
                        ret = self->read<uint64_t>(idx);
                        break;
                    case TYPE_FLOAT:
                        ret = self->read<float>(idx);
                        break;
                    case TYPE_DOUBLE:
                        ret = self->read<double>(idx);
                        break;
                    default:
                        break;
                }
                return ret;
            })
        .endClass()
        .deriveClass<ACoreObject, Named>("ACoreObject")
        .addFunction("set_conf", checked<&LuaUtilApi::configurable_set_conf<ACoreObject>>)
        .addFunction("setup", static_cast<bool (ACoreObject::*)()>(&ACoreService::setup))
        .addFunction("init", static_cast<bool (ACoreObject::*)()>(&ACoreService::init))
        .addFunction("start", static_cast<bool (ACoreObject::*)()>(&ACoreService::start))
        .addFunction("stop", static_cast<bool (ACoreObject::*)()>(&ACoreService::stop))
        .addFunction("reset", static_cast<bool (ACoreObject::*)()>(&ACoreService::reset))
        .addFunction("is_running", static_cast<bool (ACoreObject::*)() const>(&ACoreService::is_running))
        .endClass()
        .beginClass<ChannelWaiter>("ChannelWaiter")
        .addConstructor<void (*)(Channel *)>()
        .addFunction("observe", &ObserverWaiter<Channel>::observe)
        .addFunction("clear", &ObserverWaiter<Channel>::clear)
        .addFunction(
            "wait",
            +[](ChannelWaiter *self, uint32_t notifications, lua_State *state) {
                LuaGilRelease release(state);
                self->wait(notifications);
            })
        .addFunction(
            "wait_for",
            +[](ChannelWaiter *self, luabridge::LuaRef duration, uint32_t notifications, lua_State *state) {
                Duration dur = sihd::lua::to_duration(duration);
                LuaGilRelease release(state);
                return self->wait_for(dur, notifications);
            })
        // prev_* count notifications from the last wait instead of this call: use them when the same
        // thread writes then waits, else a notification landing before the wait is missed
        .addFunction(
            "prev_wait",
            +[](ChannelWaiter *self, uint32_t notifications, lua_State *state) {
                LuaGilRelease release(state);
                self->prev_wait(notifications);
            })
        .addFunction(
            "prev_wait_for",
            +[](ChannelWaiter *self, luabridge::LuaRef duration, uint32_t notifications, lua_State *state) {
                Duration dur = sihd::lua::to_duration(duration);
                LuaGilRelease release(state);
                return self->prev_wait_for(dur, notifications);
            })
        .addFunction("observing", &ObserverWaiter<Channel>::observing)
        .addFunction("notifications", &ObserverWaiter<Channel>::notifications)
        .endClass()
        .deriveClass<DevFilter, Device>("DevFilter")
        .addConstructorFrom<SmartNodePtr<DevFilter>, void(const std::string &, Node *)>()
        .endClass()
        .deriveClass<DevPulsation, Device>("DevPulsation")
        .addConstructorFrom<SmartNodePtr<DevPulsation>, void(const std::string &, Node *)>()
        .endClass()
        .deriveClass<DevSampler, Device>("DevSampler")
        .addConstructorFrom<SmartNodePtr<DevSampler>, void(const std::string &, Node *)>()
        .endClass()
        .deriveClass<DevWorker, Device>("DevWorker")
        .addConstructorFrom<SmartNodePtr<DevWorker>, void(const std::string &, Node *)>()
        .endClass()
        .deriveClass<DevWorkerPool, DevWorker>("DevWorkerPool")
        .addConstructorFrom<SmartNodePtr<DevWorkerPool>, void(const std::string &, Node *)>()
        .endClass()
        .deriveClass<DevBatcher, Device>("DevBatcher")
        .addConstructorFrom<SmartNodePtr<DevBatcher>, void(const std::string &, Node *)>()
        .endClass()
        .deriveClass<DevStats, Device>("DevStats")
        .addConstructorFrom<SmartNodePtr<DevStats>, void(const std::string &, Node *)>()
        .endClass()
        .deriveClass<DevPlayer, Device>("DevPlayer")
        .addConstructorFrom<SmartNodePtr<DevPlayer>, void(const std::string &, Node *)>()
        .endClass()
        .deriveClass<DevRecorder, Device>("DevRecorder")
        .addConstructorFrom<SmartNodePtr<DevRecorder>, void(const std::string &, Node *)>()
        .endClass()
        .deriveClass<DevMessage, Device>("DevMessage")
        .addConstructorFrom<SmartNodePtr<DevMessage>, void(const std::string &, Node *)>()
        .addFunction(
            "set_message_path",
            +[](DevMessage *self, const std::string & path) -> bool { return self->set_message_path(path); })
        .addFunction("set_trigger_mode", &DevMessage::set_trigger_mode)
        .endClass()
        .endNamespace()
        .endNamespace();
}

LuaCoreApi::LuaChannelHandler::LuaChannelHandler(lua_State *state): _state(state) {}

LuaCoreApi::LuaChannelHandler::~LuaChannelHandler() = default;

void LuaCoreApi::LuaChannelHandler::handle(Channel *c)
{
    // GIL before _mutex_map: set_observer takes the map with the GIL held
    LuaGilGuard guard(_state);
    {
        std::lock_guard lock(_mutex_map);
        auto it = _map_handler.find(c);
        if (it == _map_handler.end())
            return;
        it->second.thread_runner.call_lua_method_noret<Channel *>(c);
    }
}

void LuaCoreApi::LuaChannelHandler::add_channel_obs(Channel *c, lua_State *state, luabridge::LuaRef ref)
{
    LuaSingleChannelHandler lsch(state, ref);
    Vm current_vm(state);
    lua_State *new_thread_state = current_vm.new_luathread();
    if (new_thread_state == nullptr)
        return;
    lsch.thread_runner.new_lua_state(new_thread_state);
    {
        std::lock_guard lock(_mutex_map);
        _map_handler.insert({c, std::move(lsch)});
    }
}

void LuaCoreApi::LuaChannelHandler::remove_channel_obs(Channel *c)
{
    std::lock_guard lock(_mutex_map);
    _map_handler.erase(c);
}

} // namespace sihd::lua
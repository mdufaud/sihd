#include <pybind11/pybind11.h>

#include <sihd/core/ACoreObject.hpp>
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
#include <sihd/py/core/PyCoreApi.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/SmartNodePtr.hpp>

#include "../expected_caster.hpp"

namespace sihd::py
{

using namespace sihd::util;
using namespace sihd::core;

SIHD_LOGGER;

namespace
{

struct PySingleChannelHandler
{
        PySingleChannelHandler(pybind11::function & fun): fun(fun) {}

        pybind11::function fun;
};

std::mutex g_mutex_channel_handler_map;
std::map<sihd::core::Channel *, PySingleChannelHandler> g_single_channel_handler_map;
PyCoreApi::PyChannelHandler g_channel_handler;

// observers may call back into python
template <typename T>
bool unlock_gil_write_channel(Channel *self, size_t idx, T val)
{
    pybind11::gil_scoped_release release;
    return self->write<T>(idx, val);
}

void __attribute__((constructor)) premain()
{
    PyApi::add_api("sihd_core", &PyCoreApi::add_core_api);
}

} // namespace

void PyCoreApi::add_core_api(PyApi::PyModule & pymodule)
{
    auto m_sihd = pymodule.module();
    if (pymodule.load("sihd_util") == false)
        return;
    pybind11::module m_util = (pybind11::module)m_sihd.attr("util");

    pybind11::module m_core = m_sihd.def_submodule("core", "sihd::core");

    pybind11::class_<Channel, Named, SmartNodePtr<Channel>>(m_core, "Channel")
        .def(pybind11::init<const std::string &, const std::string &, size_t, Node *>(), pybind11::keep_alive<1, 5>())
        .def(pybind11::init<const std::string &, const std::string &, size_t>())
        .def(pybind11::init<const std::string &, const std::string &, Node *>(), pybind11::keep_alive<1, 3>())
        .def(pybind11::init<const std::string &, const std::string &>())
        .def("set_write_on_change", &Channel::set_write_on_change)
        .def("timestamp", &Channel::timestamp)
        .def("array",
             static_cast<const IArray *(Channel::*)() const>(&Channel::array),
             pybind11::return_value_policy::reference_internal)
        .def("notify", &Channel::notify, pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("size", &Channel::size)
        .def("capacity", &Channel::capacity)
        .def("reserve", &Channel::reserve)
        .def("resize", &Channel::resize)
        .def("data_size", &Channel::data_size)
        .def("data_type", &Channel::data_type)
        .def("is_same_type", static_cast<bool (Channel::*)(const Channel *) const>(&Channel::is_same_type))
        .def(
            "set_observer",
            +[](Channel *self, [[maybe_unused]] pybind11::none none) { g_channel_handler.remove_channel_obs(self); })
        .def(
            "set_observer",
            +[](Channel *self, pybind11::function fun) { g_channel_handler.add_channel_obs(self, fun); })
        .def(
            "copy_to",
            +[](Channel *self, IArray *array_ptr) { return self->copy_to(*array_ptr); },
            pybind11::call_guard<pybind11::gil_scoped_release>())
        .def(
            "write_array",
            +[](Channel *self, const IArray *array_ptr) { return self->write(*array_ptr); },
            pybind11::call_guard<pybind11::gil_scoped_release>())
        .def(
            "write",
            +[](Channel *self, size_t idx, pybind11::object arg) {
                if (self->array() == nullptr)
                    return false;
                switch (self->array()->data_type())
                {
                    case TYPE_BOOL:
                        return unlock_gil_write_channel<bool>(self, idx, arg.cast<bool>());
                    case TYPE_CHAR:
                        return unlock_gil_write_channel<char>(self, idx, arg.cast<char>());
                    case TYPE_BYTE:
                        return unlock_gil_write_channel<int8_t>(self, idx, arg.cast<int64_t>());
                    case TYPE_UBYTE:
                        return unlock_gil_write_channel<uint8_t>(self, idx, arg.cast<uint64_t>());
                    case TYPE_SHORT:
                        return unlock_gil_write_channel<int16_t>(self, idx, arg.cast<int64_t>());
                    case TYPE_USHORT:
                        return unlock_gil_write_channel<uint16_t>(self, idx, arg.cast<uint64_t>());
                    case TYPE_INT:
                        return unlock_gil_write_channel<int32_t>(self, idx, arg.cast<int64_t>());
                    case TYPE_UINT:
                        return unlock_gil_write_channel<uint32_t>(self, idx, arg.cast<uint64_t>());
                    case TYPE_LONG:
                        return unlock_gil_write_channel<int64_t>(self, idx, arg.cast<int64_t>());
                    case TYPE_ULONG:
                        return unlock_gil_write_channel<uint64_t>(self, idx, arg.cast<uint64_t>());
                    case TYPE_FLOAT:
                        return unlock_gil_write_channel<float>(self, idx, arg.cast<double>());
                    case TYPE_DOUBLE:
                        return unlock_gil_write_channel<double>(self, idx, arg.cast<double>());
                    default:
                        return false;
                }
            })
        .def(
            "read",
            +[](Channel *self, size_t idx) {
                PyObject *ret = Py_None;
                if (self->array() != nullptr)
                {
                    switch (self->array()->data_type())
                    {
                        case TYPE_BOOL:
                            ret = PyBool_FromLong(self->read<bool>(idx));
                            break;
                        case TYPE_CHAR:
                        {
                            char ch = self->read<char>(idx);
                            ret = PyUnicode_FromStringAndSize(&ch, 1);
                            break;
                        }
                        case TYPE_BYTE:
                            ret = PyLong_FromLong(self->read<int8_t>(idx));
                            break;
                        case TYPE_UBYTE:
                            ret = PyLong_FromUnsignedLong(self->read<uint8_t>(idx));
                            break;
                        case TYPE_SHORT:
                            ret = PyLong_FromLong(self->read<int16_t>(idx));
                            break;
                        case TYPE_USHORT:
                            ret = PyLong_FromUnsignedLong(self->read<uint16_t>(idx));
                            break;
                        case TYPE_INT:
                            ret = PyLong_FromLong(self->read<int32_t>(idx));
                            break;
                        case TYPE_UINT:
                            ret = PyLong_FromUnsignedLong(self->read<uint32_t>(idx));
                            break;
                        case TYPE_LONG:
                            ret = PyLong_FromLongLong(self->read<int64_t>(idx));
                            break;
                        case TYPE_ULONG:
                            ret = PyLong_FromUnsignedLongLong(self->read<uint64_t>(idx));
                            break;
                        case TYPE_FLOAT:
                            ret = PyFloat_FromDouble(self->read<float>(idx));
                            break;
                        case TYPE_DOUBLE:
                            ret = PyFloat_FromDouble(self->read<double>(idx));
                            break;
                        default:
                            break;
                    }
                }
                return pybind11::reinterpret_steal<pybind11::object>(ret);
            });

    pybind11::class_<Device, Node, Configurable, SmartNodePtr<Device>>(m_core, "Device")
        .def(
            "find_channel",
            +[](Device *self, const std::string & name) { return self->find_channel(name); },
            pybind11::call_guard<pybind11::gil_scoped_release>(),
            pybind11::return_value_policy::reference_internal)
        .def(
            "get_channel",
            +[](Device *self, const std::string & name) { return self->get_channel(name); },
            pybind11::call_guard<pybind11::gil_scoped_release>(),
            pybind11::return_value_policy::reference_internal)
        .def("add_channel",
             static_cast<Channel *(Device::*)(const std::string &, std::string_view, size_t)>(&Device::add_channel),
             pybind11::return_value_policy::reference_internal)
        .def("setup",
             static_cast<bool (Device::*)()>(&ACoreService::setup),
             pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("init",
             static_cast<bool (Device::*)()>(&ACoreService::init),
             pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("start",
             static_cast<bool (Device::*)()>(&ACoreService::start),
             pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("stop",
             static_cast<bool (Device::*)()>(&ACoreService::stop),
             pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("reset",
             static_cast<bool (Device::*)()>(&ACoreService::reset),
             pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("is_running", static_cast<bool (Device::*)() const>(&ACoreService::is_running))
        .def("device_state", &Device::device_state, pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("device_state_str", &Device::device_state_str, pybind11::call_guard<pybind11::gil_scoped_release>())
        .def(
            "service_ctrl",
            // service_ctrl() is public on AService (protected override on Device)
            [](Device & self) {
                return dynamic_cast<ServiceController *>(static_cast<AService &>(self).service_ctrl());
            },
            pybind11::return_value_policy::reference_internal);

    pybind11::class_<Core, Device, SmartNodePtr<Core>>(m_core, "Core")
        .def(pybind11::init<const std::string &, Node *>(), pybind11::keep_alive<1, 3>())
        .def(pybind11::init<const std::string &>());

    pybind11::class_<ACoreObject, Named, Configurable>(m_core, "ACoreObject")
        .def("setup",
             static_cast<bool (ACoreObject::*)()>(&ACoreService::setup),
             pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("init",
             static_cast<bool (ACoreObject::*)()>(&ACoreService::init),
             pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("start",
             static_cast<bool (ACoreObject::*)()>(&ACoreService::start),
             pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("stop",
             static_cast<bool (ACoreObject::*)()>(&ACoreService::stop),
             pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("reset",
             static_cast<bool (ACoreObject::*)()>(&ACoreService::reset),
             pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("is_running", static_cast<bool (ACoreObject::*)() const>(&ACoreService::is_running));

    pybind11::class_<ChannelWaiter>(m_core, "ChannelWaiter")
        .def(pybind11::init<Channel *>(), pybind11::keep_alive<1, 2>())
        .def("observe", &ObserverWaiter<Channel>::observe)
        .def("clear", &ObserverWaiter<Channel>::clear)
        .def("wait", &ObserverWaiter<Channel>::wait, pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("wait_for", &ObserverWaiter<Channel>::wait_for, pybind11::call_guard<pybind11::gil_scoped_release>())
        // prev_*: count notifications since the last wait, for a thread that writes then waits
        .def("prev_wait", &ObserverWaiter<Channel>::prev_wait, pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("prev_wait_for",
             &ObserverWaiter<Channel>::prev_wait_for,
             pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("observing", &ObserverWaiter<Channel>::observing)
        .def("notifications",
             &ObserverWaiter<Channel>::notifications,
             pybind11::call_guard<pybind11::gil_scoped_release>());

    pybind11::class_<DevFilter, Device, SmartNodePtr<DevFilter>>(m_core, "DevFilter")
        .def(pybind11::init<const std::string &, Node *>(), pybind11::keep_alive<1, 3>())
        .def(pybind11::init<const std::string &>());

    pybind11::class_<DevPulsation, Device, SmartNodePtr<DevPulsation>>(m_core, "DevPulsation")
        .def(pybind11::init<const std::string &, Node *>(), pybind11::keep_alive<1, 3>())
        .def(pybind11::init<const std::string &>());

    pybind11::class_<DevSampler, Device, SmartNodePtr<DevSampler>>(m_core, "DevSampler")
        .def(pybind11::init<const std::string &, Node *>(), pybind11::keep_alive<1, 3>())
        .def(pybind11::init<const std::string &>());

    pybind11::class_<DevWorker, Device, SmartNodePtr<DevWorker>>(m_core, "DevWorker")
        .def(pybind11::init<const std::string &, Node *>(), pybind11::keep_alive<1, 3>())
        .def(pybind11::init<const std::string &>());

    pybind11::class_<DevWorkerPool, DevWorker, SmartNodePtr<DevWorkerPool>>(m_core, "DevWorkerPool")
        .def(pybind11::init<const std::string &, Node *>(), pybind11::keep_alive<1, 3>())
        .def(pybind11::init<const std::string &>());

    pybind11::class_<DevBatcher, Device, SmartNodePtr<DevBatcher>>(m_core, "DevBatcher")
        .def(pybind11::init<const std::string &, Node *>(), pybind11::keep_alive<1, 3>())
        .def(pybind11::init<const std::string &>());

    pybind11::class_<DevStats, Device, SmartNodePtr<DevStats>>(m_core, "DevStats")
        .def(pybind11::init<const std::string &, Node *>(), pybind11::keep_alive<1, 3>())
        .def(pybind11::init<const std::string &>());

    pybind11::class_<DevPlayer, Device, SmartNodePtr<DevPlayer>>(m_core, "DevPlayer")
        .def(pybind11::init<const std::string &, Node *>(), pybind11::keep_alive<1, 3>())
        .def(pybind11::init<const std::string &>());

    pybind11::class_<DevRecorder, Device, SmartNodePtr<DevRecorder>>(m_core, "DevRecorder")
        .def(pybind11::init<const std::string &, Node *>(), pybind11::keep_alive<1, 3>())
        .def(pybind11::init<const std::string &>());

    pybind11::class_<DevMessage, Device, SmartNodePtr<DevMessage>>(m_core, "DevMessage")
        .def(pybind11::init<const std::string &, Node *>(), pybind11::keep_alive<1, 3>())
        .def(pybind11::init<const std::string &>())
        .def("set_message_path", &DevMessage::set_message_path)
        .def("set_trigger_mode", &DevMessage::set_trigger_mode);
}

PyCoreApi::PyChannelHandler::PyChannelHandler() = default;

PyCoreApi::PyChannelHandler::~PyChannelHandler() = default;

void PyCoreApi::PyChannelHandler::handle(Channel *channel)
{
    // GIL must cover copy, call and destruction of the python function: all touch its refcount
    pybind11::gil_scoped_acquire acquire;
    pybind11::function fun;
    {
        std::lock_guard lock(g_mutex_channel_handler_map);
        auto it = g_single_channel_handler_map.find(channel);
        if (it == g_single_channel_handler_map.end())
        {
            channel->remove_observer(&g_channel_handler);
            return;
        }
        fun = it->second.fun;
    }
    fun(channel);
}

void PyCoreApi::PyChannelHandler::add_channel_obs(Channel *channel, pybind11::function & fun)
{
    {
        std::lock_guard lock(g_mutex_channel_handler_map);
        g_single_channel_handler_map.insert({channel, fun});
    }
    pybind11::gil_scoped_release release;
    channel->add_observer(&g_channel_handler);
}

void PyCoreApi::PyChannelHandler::remove_channel_obs(Channel *channel)
{
    {
        std::lock_guard lock(g_mutex_channel_handler_map);
        g_single_channel_handler_map.erase(channel);
    }
    pybind11::gil_scoped_release release;
    channel->remove_observer(&g_channel_handler);
}

} // namespace sihd::py
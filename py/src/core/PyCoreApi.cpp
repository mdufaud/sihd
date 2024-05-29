#include <sihd/py/core/PyCoreApi.hpp>

#include <sihd/util/Logger.hpp>
#include <sihd/util/SmartNodePtr.hpp>

#include <sihd/core/ACoreObject.hpp>
#include <sihd/core/ChannelWaiter.hpp>
#include <sihd/core/Core.hpp>
#include <sihd/core/DevFilter.hpp>
#include <sihd/core/DevPlayer.hpp>
#include <sihd/core/DevPulsation.hpp>
#include <sihd/core/DevRecorder.hpp>
#include <sihd/core/DevSampler.hpp>
#include <sihd/core/Device.hpp>

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
             static_cast<const sihd::util::IArray *(Channel::*)() const>(&Channel::array),
             pybind11::return_value_policy::reference_internal)
        .def("notify", &Channel::notify, pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("size", &Channel::size)
        .def("data_size", &Channel::data_size)
        .def("data_type", &Channel::data_type)
        .def("is_same_type", static_cast<bool (Channel::*)(const Channel *) const>(&Channel::is_same_type))
        .def(
            "set_observer",
            +[](Channel *self, [[maybe_unused]] pybind11::none none) { g_channel_handler.remove_channel_obs(self); },
            pybind11::call_guard<pybind11::gil_scoped_release>())
        .def(
            "set_observer",
            +[](Channel *self, pybind11::function fun) { g_channel_handler.add_channel_obs(self, fun); },
            pybind11::call_guard<pybind11::gil_scoped_release>())
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
                        return PyCoreApi::_unlock_gil_write_channel<bool>(self, idx, arg.cast<bool>());
                    case TYPE_CHAR:
                        return PyCoreApi::_unlock_gil_write_channel<char>(self, idx, arg.cast<char>());
                    case TYPE_BYTE:
                        return PyCoreApi::_unlock_gil_write_channel<int8_t>(self, idx, arg.cast<int64_t>());
                    case TYPE_UBYTE:
                        return PyCoreApi::_unlock_gil_write_channel<uint8_t>(self, idx, arg.cast<uint64_t>());
                    case TYPE_SHORT:
                        return PyCoreApi::_unlock_gil_write_channel<int16_t>(self, idx, arg.cast<int64_t>());
                    case TYPE_USHORT:
                        return PyCoreApi::_unlock_gil_write_channel<uint16_t>(self, idx, arg.cast<uint64_t>());
                    case TYPE_INT:
                        return PyCoreApi::_unlock_gil_write_channel<int32_t>(self, idx, arg.cast<int64_t>());
                    case TYPE_UINT:
                        return PyCoreApi::_unlock_gil_write_channel<uint32_t>(self, idx, arg.cast<uint64_t>());
                    case TYPE_LONG:
                        return PyCoreApi::_unlock_gil_write_channel<int64_t>(self, idx, arg.cast<int64_t>());
                    case TYPE_ULONG:
                        return PyCoreApi::_unlock_gil_write_channel<uint64_t>(self, idx, arg.cast<uint64_t>());
                    case TYPE_FLOAT:
                        return PyCoreApi::_unlock_gil_write_channel<float>(self, idx, arg.cast<double>());
                    case TYPE_DOUBLE:
                        return PyCoreApi::_unlock_gil_write_channel<double>(self, idx, arg.cast<double>());
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
                            char c = self->read<char>(idx);
                            ret = PyUnicode_FromStringAndSize(&c, 1);
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
        // AChannelContainer
        .def("find_channel",
             static_cast<Channel *(Device::*)(const std::string &)>(&Device::find_channel),
             pybind11::call_guard<pybind11::gil_scoped_release>(),
             pybind11::return_value_policy::reference_internal)
        .def("get_channel",
             static_cast<Channel *(Device::*)(const std::string &)>(&Device::get_channel),
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
        .def("device_state_str", &Device::device_state_str, pybind11::call_guard<pybind11::gil_scoped_release>());

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
        .def("observe", &sihd::util::ObserverWaiter<Channel>::observe)
        .def("clear", &sihd::util::ObserverWaiter<Channel>::clear)
        .def("wait", &sihd::util::ObserverWaiter<Channel>::wait, pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("wait_for",
             &sihd::util::ObserverWaiter<Channel>::wait_for,
             pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("observing", &sihd::util::ObserverWaiter<Channel>::observing)
        .def("notifications",
             &sihd::util::ObserverWaiter<Channel>::notifications,
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

    pybind11::class_<DevPlayer, Device, SmartNodePtr<DevPlayer>>(m_core, "DevPlayer")
        .def(pybind11::init<const std::string &, Node *>(), pybind11::keep_alive<1, 3>())
        .def(pybind11::init<const std::string &>());

    pybind11::class_<DevRecorder, Device, SmartNodePtr<DevRecorder>>(m_core, "DevRecorder")
        .def(pybind11::init<const std::string &, Node *>(), pybind11::keep_alive<1, 3>())
        .def(pybind11::init<const std::string &>());
}

static void __attribute__((constructor)) premain()
{
    PyApi::add_api("sihd_core", &PyCoreApi::add_core_api);
}

/* ************************************************************************* */
/* PyChannelHandler */
/* ************************************************************************* */

PyCoreApi::PyChannelHandler::PyChannelHandler() {}

PyCoreApi::PyChannelHandler::~PyChannelHandler() {}

void PyCoreApi::PyChannelHandler::handle(Channel *channel)
{
    pybind11::function fun;
    {
        std::lock_guard l(g_mutex_channel_handler_map);
        auto it = g_single_channel_handler_map.find(channel);
        if (it == g_single_channel_handler_map.end())
        {
            channel->remove_observer_inside_notification(&g_channel_handler);
            return;
        }
        fun = it->second.fun;
    }
    pybind11::gil_scoped_acquire acquire;
    fun(channel);
}

void PyCoreApi::PyChannelHandler::add_channel_obs(Channel *channel, pybind11::function & fun)
{
    {
        std::lock_guard l(g_mutex_channel_handler_map);
        g_single_channel_handler_map.insert({channel, fun});
    }
    channel->add_observer(&g_channel_handler);
}

void PyCoreApi::PyChannelHandler::remove_channel_obs(Channel *channel)
{
    {
        std::lock_guard l(g_mutex_channel_handler_map);
        g_single_channel_handler_map.erase(channel);
    }
    channel->remove_observer(&g_channel_handler);
}

} // namespace sihd::py
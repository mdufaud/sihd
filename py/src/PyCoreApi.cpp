#include <sihd/py/PyCoreApi.hpp>

namespace sihd::py
{

using namespace sihd::util;
using namespace sihd::core;

SIHD_LOGGER;

PyCoreApi::PyChannelHandler PyCoreApi::_channel_handler;

void    PyCoreApi::add_core_api(PyApi::PyModule & pymodule)
{
    auto m_sihd = pymodule.module();
    if (pymodule.load("sihd_util") == false)
        return ;
    pybind11::module m_util = (pybind11::module)m_sihd.attr("util");

    pybind11::module m_core = m_sihd.def_submodule("core", "sihd::core");

    pybind11::class_<Channel, Named, SmartNodePtr<Channel>>(m_core, "Channel")
        .def(pybind11::init<const std::string &, const std::string &, size_t, Node *>())
        .def(pybind11::init<const std::string &, const std::string &, Node *>())
        .def(pybind11::init<const std::string &, const std::string &, size_t>())
        .def(pybind11::init<const std::string &, const std::string &>())
        .def("set_write_on_change", &Channel::set_write_on_change)
        .def("notify", &Channel::notify)
        .def("timestamp", &Channel::timestamp)
        .def("array", static_cast<sihd::util::IArray *(Channel::*)()>(&Channel::array), pybind11::return_value_policy::reference_internal)
        .def("set_observer", +[] (Channel *self, pybind11::none none)
        {
            (void)none;
            PyCoreApi::_channel_handler.remove_channel_obs(self);
            self->remove_observer(&PyCoreApi::_channel_handler);
        })
        .def("set_observer", +[] (Channel *self, pybind11::function fun)
        {
            PyCoreApi::_channel_handler.add_channel_obs(self, fun);
            self->add_observer(&PyCoreApi::_channel_handler);
        })
        .def("copy_to", +[] (Channel *self, IArray *array_ptr)
        {
            return self->copy_to(*array_ptr);
        })
        .def("write_array", +[] (Channel *self, const IArray *array_ptr)
        {
            return self->write(*array_ptr);
        })
        .def("write", +[] (Channel *self, size_t idx, pybind11::object arg)
        {
            if (self->array() == nullptr)
                return false;
            switch (self->array()->data_type())
            {
                case TYPE_BOOL:
                    return self->write<bool>(idx, arg.cast<bool>());
                case TYPE_CHAR:
                    return self->write<char>(idx, arg.cast<char>());
                case TYPE_BYTE:
                    return self->write<int8_t>(idx, arg.cast<int64_t>());
                case TYPE_UBYTE:
                    return self->write<uint8_t>(idx, arg.cast<uint64_t>());
                case TYPE_SHORT:
                    return self->write<int16_t>(idx, arg.cast<int64_t>());
                case TYPE_USHORT:
                    return self->write<uint16_t>(idx, arg.cast<uint64_t>());
                case TYPE_INT:
                    return self->write<int32_t>(idx, arg.cast<int64_t>());
                case TYPE_UINT:
                    return self->write<uint32_t>(idx, arg.cast<uint64_t>());
                case TYPE_LONG:
                    return self->write<int64_t>(idx, arg.cast<int64_t>());
                case TYPE_ULONG:
                    return self->write<uint64_t>(idx, arg.cast<uint64_t>());
                case TYPE_FLOAT:
                    return self->write<float>(idx, arg.cast<double>());
                case TYPE_DOUBLE:
                    return self->write<double>(idx, arg.cast<double>());
                default:
                    return false;
            }
        })
        .def("read", +[] (Channel *self, size_t idx)
        {
            PyObject *ret = Py_None;
            if (self->array() != nullptr)
            {
                switch (self->array()->data_type())
                {
                    case TYPE_BOOL:
                        ret = PyBool_FromLong(self->read<bool>(idx));
                        break ;
                    case TYPE_CHAR:
                    {
                        char c = self->read<char>(idx);
                        ret = PyUnicode_FromStringAndSize(&c, 1);
                        break ;
                    }
                    case TYPE_BYTE:
                        ret = PyLong_FromLong(self->read<int8_t>(idx));
                        break ;
                    case TYPE_UBYTE:
                        ret = PyLong_FromUnsignedLong(self->read<uint8_t>(idx));
                        break ;
                    case TYPE_SHORT:
                        ret = PyLong_FromLong(self->read<int16_t>(idx));
                        break ;
                    case TYPE_USHORT:
                        ret = PyLong_FromUnsignedLong(self->read<uint16_t>(idx));
                        break ;
                    case TYPE_INT:
                        ret = PyLong_FromLong(self->read<int32_t>(idx));
                        break ;
                    case TYPE_UINT:
                        ret = PyLong_FromUnsignedLong(self->read<uint32_t>(idx));
                        break ;
                    case TYPE_LONG:
                        ret = PyLong_FromLongLong(self->read<int64_t>(idx));
                        break ;
                    case TYPE_ULONG:
                        ret = PyLong_FromUnsignedLongLong(self->read<uint64_t>(idx));
                        break ;
                    case TYPE_FLOAT:
                        ret = PyFloat_FromDouble(self->read<float>(idx));
                        break ;
                    case TYPE_DOUBLE:
                        ret = PyFloat_FromDouble(self->read<double>(idx));
                        break ;
                    default:
                        break ;
                }
            }
            return pybind11::reinterpret_steal<pybind11::object>(ret);
        });
}

static void __attribute__ ((constructor)) premain()
{
    PyApi::add_api("sihd_core", &PyCoreApi::add_core_api);
}

/* ************************************************************************* */
/* PyChannelHandler */
/* ************************************************************************* */

PyCoreApi::PyChannelHandler::PyChannelHandler()
{
}

PyCoreApi::PyChannelHandler::~PyChannelHandler()
{
}

void    PyCoreApi::PyChannelHandler::handle(Channel *c)
{
    {
        std::lock_guard l(_mutex_map);
        // find lua method
        auto it = _map_handler.find(c);
        if (it == _map_handler.end())
            return ;
        // handle channel
        pybind11::gil_scoped_acquire acquire;
        it->second(c);
    }
}

void    PyCoreApi::PyChannelHandler::add_channel_obs(Channel *c, pybind11::function & fun)
{
    {
        // set infos to handling map
        std::lock_guard l(_mutex_map);
        _map_handler.insert({c, fun});
    }
}

void    PyCoreApi::PyChannelHandler::remove_channel_obs(Channel *c)
{
    std::lock_guard l(_mutex_map);
    _map_handler.erase(c);
}

}
#include <sihd/py/PyUtilApi.hpp>

#include <sihd/util/Files.hpp>
#include <sihd/util/OS.hpp>

#include <sihd/util/Configurable.hpp>
#include <sihd/util/Scheduler.hpp>
#include <sihd/util/Splitter.hpp>

#include <sihd/util/version.hpp>
#include <sihd/util/Types.hpp>
#include <sihd/util/time.hpp>
#include <sihd/util/Thread.hpp>
#include <sihd/util/Path.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/AService.hpp>
#include <sihd/util/ServiceController.hpp>

// .def("push_back", &LuaUtilApi::_array_lua_push_back<PrimitiveType>)
// .def("copy_from", &LuaUtilApi::_array_lua_copy_table<PrimitiveType>)

#define DECLARE_ARRAY_USERTYPE(ArrType, PrimitiveType) \
    pybind11::class_<ArrType, IArray>(m_util, #ArrType) \
        .def(pybind11::init<>()) \
        .def(pybind11::init<size_t>()) \
        .def(pybind11::init<const std::vector<PrimitiveType>>()) \
        .def("clone", &ArrType::clone) \
        .def("pop", &ArrType::pop) \
        .def("front", &ArrType::front) \
        .def("back", &ArrType::back) \
        .def("at", &ArrType::at) \
        .def("to_string", &ArrType::to_string) \
        .def("__getitem__", &PyUtilApi::_array_py_getitem<PrimitiveType>) \
        .def("__setitem__", &ArrType::set) \
        .def("__contains__", &PyUtilApi::_array_py_contains<PrimitiveType>) \
        .def("__reversed__", &PyUtilApi::_array_py_reversed<PrimitiveType>) \
        .def("__iter__", &PyUtilApi::_array_py_iter<PrimitiveType>, pybind11::keep_alive<0, 1>());

namespace sihd::py
{

using namespace sihd::util;

// from path/bin/exe.lua -> path/bin -> path
std::string PyUtilApi::dir = Files::get_parent(Files::get_parent(OS::get_executable_path()));

// global api logger
SIHD_NEW_LOGGER("sihd::py::api");

// logger used by python code
Logger PyUtilApi::logger("sihd::py");

void    PyUtilApi::add_util_api(PyApi::PyModule & pymodule)
{
    auto m_sihd = pymodule.module();

    m_sihd.attr("version") = SIHD_VERSION_STRING;
    m_sihd.attr("version_num") = SIHD_VERSION_NUM;
    m_sihd.attr("version_major") = SIHD_VERSION_MAJOR;
    m_sihd.attr("version_minor") = SIHD_VERSION_MINOR;
    m_sihd.attr("version_patch") = SIHD_VERSION_PATCH;

    pybind11::module m_util = m_sihd.def_submodule("util", "sihd::util");

    m_util.def_submodule("log", "sihd::util::Logger")
        .def("debug", +[] (std::string_view log) { PyUtilApi::logger.log(debug, log); })
        .def("info", +[] (std::string_view log) { PyUtilApi::logger.log(info, log); })
        .def("warning", +[] (std::string_view log) { PyUtilApi::logger.log(warning, log); })
        .def("error", +[] (std::string_view log) { PyUtilApi::logger.log(error, log); })
        .def("critical", +[] (std::string_view log) { PyUtilApi::logger.log(critical, log); });

    m_util.def_submodule("types", "sihd::util::Types")
        .def("type_size", &Types::type_size)
        .def("type_to_string", &Types::type_to_string)
        .def("string_to_type", &Types::string_to_type);

    m_util.def_submodule("thread", "sihd::util::Thread")
        .def("id", &Thread::id)
        .def("id_str", static_cast<std::string (*)()>(&Thread::id_str))
        .def("set_name", &Thread::set_name)
        .def("get_name", &Thread::get_name);

    m_util.def_submodule("path", "sihd::util::Path")
        .def("set", &Path::set)
        .def("clear", &Path::clear)
        .def("clear_all", &Path::clear_all)
        .def("get", static_cast<std::string (*)(const std::string &)>(&Path::get))
        .def("get_from_url", static_cast<std::string (*)(const std::string &, const std::string &)>(&Path::get))
        .def("get_from_path", &Path::get_from)
        .def("find", &Path::find);

    m_util.def_submodule("time", "sihd::util::time")
        // sleep
        .def("nsleep", &time::nsleep)
        .def("usleep", &time::usleep)
        .def("msleep", &time::msleep)
        .def("sleep", &time::sleep)
        // convert from nano
        .def("to_us", &time::to_micro)
        .def("to_ms", &time::to_milli)
        .def("to_sec", &time::to_sec)
        .def("to_min", &time::to_min)
        .def("to_hours", &time::to_hours)
        .def("to_days", &time::to_days)
        .def("to_double", &time::to_double)
        .def("to_hz", &time::to_freq)
        // convert to nano
        .def("us", &time::micro)
        .def("ms", &time::milli)
        .def("sec", &time::sec)
        .def("min", &time::min)
        .def("hours", &time::hours)
        .def("days", &time::days)
        .def("from_double", &time::from_double)
        .def("hz", &time::freq);

    pybind11::class_<Splitter>(m_util, "Splitter")
        .def(pybind11::init<>())
        .def("set_empty_delimitations", &Splitter::set_empty_delimitations)
        .def("set_delimiter", &Splitter::set_delimiter)
        .def("set_delimiter_spaces", &Splitter::set_delimiter_spaces)
        .def("set_escape_sequences", &Splitter::set_escape_sequences)
        .def("set_escape_sequences_all", &Splitter::set_escape_sequences_all)
        .def("split", &Splitter::split)
        .def("count_tokens", &Splitter::count_tokens);

    pybind11::class_<Configurable>(m_util, "Configurable")
        .def("set_conf", +[] (Configurable & self, const pybind11::dict & conf)
        {
            std::string key;
            bool ret = true;
            for (const auto & item: conf)
            {
                try
                {
                    key = pybind11::str(item.first);
                }
                catch (const pybind11::cast_error & e)
                {
                    throw std::runtime_error("configuration key is not a string");
                }
                try
                {
                    ret = self.set_conf_str(key, item.second.cast<std::string>()) && ret;
                    continue ;
                }
                catch (const pybind11::cast_error & e)
                {
                }
                PyObject *val = item.second.ptr();
                if (PyBool_Check(val))
                    ret = self.set_conf<bool>(key, item.second.cast<bool>()) && ret;
                else if (PyLong_Check(val))
                    ret = self.set_conf_int(key, item.second.cast<int64_t>()) && ret;
                else if (PyFloat_Check(val))
                    ret = self.set_conf_float(key, item.second.cast<double>()) && ret;
                else
                    throw std::runtime_error(Str::format("configuration '%s' type error", key.c_str()).c_str());
            }
            return ret;
        });

    pybind11::class_<Named, SmartNodePtr<Named>>(m_util, "Named")
        // keep_alive 1 -> this | 2 -> first arg | 3 -> parent
        // keeps alive parent while named object is alive
        .def(pybind11::init<const std::string &, Node *>(), pybind11::keep_alive<1, 3>())
        .def(pybind11::init<const std::string &>())
        .def_property_readonly("c_ptr", +[] (const Named *self) -> int64_t { return (int64_t)self; })
        .def("get_parent", &Named::get_parent)
        .def("get_name", &Named::get_name)
        .def("get_full_name", &Named::get_full_name)
        .def("get_class_name", &Named::get_class_name)
        .def("get_description", &Named::get_description)
        .def("get_full_name", &Named::get_full_name)
        .def("get_root", &Named::get_root)
        .def("find", static_cast<Named * (Named::*)(const std::string &)>(&Named::find))
        .def("find_from", static_cast<Named * (Named::*)(Named *, const std::string &)>(&Named::find));

    pybind11::class_<Node::ChildEntry>(m_util, "ChildEntry")
        .def_readonly("name", &Node::ChildEntry::name)
        .def_readonly("obj", &Node::ChildEntry::obj)
        .def_readonly("ownership", &Node::ChildEntry::ownership);

    pybind11::class_<Node, Named, SmartNodePtr<Node>>(m_util, "Node")
        // keeps alive parent while node object is alive
        .def(pybind11::init<const std::string &, Node *>(), pybind11::keep_alive<1, 3>())
        .def(pybind11::init<const std::string &>())
        .def("get_child", static_cast<Named *(Node::*)(const std::string &) const>(&Node::get_child))
        .def("add_child", +[] (Node *self, Named *child) { return self->add_child(child); })
        .def("add_child_name", +[] (Node *self, const std::string & name, Named *child) { return self->add_child(name, child); })
        // .def("delete_child", static_cast<bool (Node::*)(const Named *)>(&Node::delete_child))
        // .def("delete_child_name", static_cast<bool (Node::*)(const std::string &)>(&Node::delete_child))
        .def("is_link", &Node::is_link)
        .def("add_link", &Node::add_link)
        .def("remove_link", &Node::remove_link)
        .def("get_link", &Node::get_link)
        .def("resolve_links", &Node::resolve_links)
        .def("get_tree_str", static_cast<std::string (Node::*)() const>(&Node::get_tree_str))
        .def("get_tree_desc_str", &Node::get_tree_desc_str)
        // ties lifetime of return to 'this'
        .def("get_children", &Node::get_children, pybind11::return_value_policy::reference_internal)
        .def("get_children_keys", &Node::get_children_keys, pybind11::return_value_policy::reference_internal);

    pybind11::class_<ServiceController>(m_util, "ServiceController")
        .def("get_state", &ServiceController::get_state);

    pybind11::class_<AService>(m_util, "AService")
        .def("setup", &AService::setup)
        .def("init", &AService::init)
        .def("start", &AService::start)
        .def("stop", &AService::stop)
        .def("reset", &AService::reset)
        .def("is_running", &AService::is_running)
        .def("get_service_ctrl", &AService::get_service_ctrl);

    pybind11::class_<IClock>(m_util, "IClock")
        .def("is_steady", &IClock::is_steady)
        .def("start", &IClock::start)
        .def("stop", &IClock::stop)
        .def("now", &IClock::now);

    pybind11::class_<SystemClock, IClock>(m_util, "SystemClock")
        .def(pybind11::init<>());

    pybind11::class_<SteadyClock, IClock>(m_util, "SteadyClock")
        .def(pybind11::init<>());

    m_util.attr("clock") = &Clock::default_clock;

    pybind11::class_<Scheduler, Named, Configurable, SmartNodePtr<Scheduler>>(m_util, "Scheduler")
        .def(pybind11::init<const std::string &, Node *>(), pybind11::keep_alive<1, 3>())
        .def(pybind11::init<const std::string &>())
        .def("get_clock", &Scheduler::get_clock)
        .def("set_clock", &Scheduler::set_clock)
        .def("start", &Scheduler::start)
        .def("stop", &Scheduler::stop)
        .def("is_running", &Scheduler::is_running)
        .def("pause", &Scheduler::pause)
        .def("resume", &Scheduler::resume)
        .def("add_task", +[] (Scheduler & self, const pybind11::function & task, time_t when, time_t repeat)
        {
            self.add_task(new PyUtilApi::PyTask(task, when, repeat));
        }, pybind11::arg("task"), pybind11::arg("when") = 0, pybind11::arg("repeat") = 0)
        .def("clear_tasks", &Scheduler::clear_tasks)
        .def_readonly("overruns", &Scheduler::overruns)
        .def_readwrite("overrun_at", &Scheduler::overrun_at)
        .def_readwrite("acceptable_nano", &Scheduler::acceptable_nano);

    pybind11::class_<IArray>(m_util, "IArray")
        .def("size", &IArray::size)
        .def("capacity", &IArray::capacity)
        .def("data_size", &IArray::data_size)
        .def("byte_size", &IArray::byte_size)
        .def("resize", &IArray::resize)
        .def("reserve", &IArray::reserve)
        .def("byte_resize", &IArray::byte_resize)
        .def("byte_reserve", &IArray::byte_reserve)
        .def("data_type", &IArray::data_type)
        .def("data_type_to_string", &IArray::data_type_to_string)
        .def("hexdump", &IArray::hexdump)
        .def("to_string", &IArray::to_string)
        .def("clear", &IArray::clear)
        .def("__len__", &IArray::size);
    DECLARE_ARRAY_USERTYPE(ArrBool, bool)
    DECLARE_ARRAY_USERTYPE(ArrChar, char)
    DECLARE_ARRAY_USERTYPE(ArrByte, int8_t)
    DECLARE_ARRAY_USERTYPE(ArrUByte, uint8_t)
    DECLARE_ARRAY_USERTYPE(ArrShort, int16_t)
    DECLARE_ARRAY_USERTYPE(ArrUShort, uint16_t)
    DECLARE_ARRAY_USERTYPE(ArrInt, int32_t)
    DECLARE_ARRAY_USERTYPE(ArrUInt, uint32_t)
    DECLARE_ARRAY_USERTYPE(ArrLong, int64_t)
    DECLARE_ARRAY_USERTYPE(ArrULong, uint64_t)
    DECLARE_ARRAY_USERTYPE(ArrFloat, float)
    DECLARE_ARRAY_USERTYPE(ArrDouble, double)
    pybind11::class_<ArrStr, IArray>(m_util, "ArrStr")
        .def(pybind11::init<>())
        .def(pybind11::init<size_t>())
        .def(pybind11::init<const std::string &>())
        .def("clone", +[] (ArrStr *self)
        {
            return self->clone();
        })
        .def("pop", &ArrStr::pop)
        .def("front", &ArrStr::front)
        .def("back", &ArrStr::back)
        .def("at", &ArrStr::at)
        .def("str", &ArrStr::cpp_str)
        .def("__getitem__", &PyUtilApi::_array_py_getitem<char>)
        .def("__setitem__", &ArrStr::set)
        .def("__contains__", &PyUtilApi::_array_py_contains<char>)
        .def("__reversed__", &PyUtilApi::_array_py_reversed<char>)
        .def("__iter__", &PyUtilApi::_array_py_iter<char>, pybind11::keep_alive<0, 1>());
}

static void __attribute__ ((constructor)) premain()
{
    PyApi::add_api("sihd_util", &PyUtilApi::add_util_api);
}

/* ************************************************************************* */
/* PyTask */
/* ************************************************************************* */

PyUtilApi::PyTask::PyTask(const pybind11::function & task,
                                time_t timestamp_to_run,
                                time_t reschedule_every):
    Task(nullptr, timestamp_to_run, reschedule_every), _fun(task)
{
}

PyUtilApi::PyTask::~PyTask()
{
}

bool    PyUtilApi::PyTask::run()
{
    pybind11::gil_scoped_acquire acquire;
    pybind11::object ret = _fun();
    return ret.cast<bool>();
}

}

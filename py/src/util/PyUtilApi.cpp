#include <sihd/py/util/PyUtilApi.hpp>

#include <sihd/util/fs.hpp>
#include <sihd/util/os.hpp>

#include <sihd/util/Scheduler.hpp>
#include <sihd/util/Splitter.hpp>
#include <sihd/util/Waitable.hpp>

#include <sihd/util/AService.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/ServiceController.hpp>
#include <sihd/util/path.hpp>
#include <sihd/util/thread.hpp>
#include <sihd/util/time.hpp>
#include <sihd/util/type.hpp>
#include <sihd/util/version.hpp>

#define DECLARE_ARRAY_USERTYPE(ArrType, PrimitiveType)                                                       \
    pybind11::class_<ArrType, IArray>(m_util, #ArrType)                                                      \
        .def(pybind11::init<>())                                                                             \
        .def(pybind11::init<size_t>())                                                                       \
        .def(pybind11::init<const std::vector<PrimitiveType>>())                                             \
        .def("clone", &ArrType::clone)                                                                       \
        .def("push_back", static_cast<bool (ArrType::*)(const PrimitiveType &)>(&ArrType::push_back))        \
        .def("push_back", &PyUtilApi::_array_py_push_back_list<PrimitiveType>)                               \
        .def("push_back", &PyUtilApi::_array_py_push_back_tuple<PrimitiveType>)                              \
        .def("push_front", static_cast<bool (ArrType::*)(const PrimitiveType &)>(&ArrType::push_front))      \
        .def("push_front", &PyUtilApi::_array_py_push_front_list<PrimitiveType>)                             \
        .def("push_front", &PyUtilApi::_array_py_push_front_tuple<PrimitiveType>)                            \
        .def("pop", &ArrType::pop)                                                                           \
        .def("front", &ArrType::front)                                                                       \
        .def("back", &ArrType::back)                                                                         \
        .def("at", &ArrType::at)                                                                             \
        .def("__setitem__", &ArrType::set)                                                                   \
        .def("__getitem__", &PyUtilApi::_array_py_getitem<PrimitiveType>)                                    \
        .def("__contains__", &PyUtilApi::_array_py_contains<PrimitiveType>)                                  \
        .def("__reversed__", &PyUtilApi::_array_py_reversed<PrimitiveType>)                                  \
        .def("__iter__", &PyUtilApi::_array_py_iter<PrimitiveType>, pybind11::keep_alive<0, 1>());

namespace sihd::py
{

using namespace sihd::util;
// global api logger
SIHD_NEW_LOGGER("sihd::py::api");

namespace
{
// logger used by python code
Logger g_py_logger("sihd::py");
// from path/bin/script.py -> path/bin -> path
std::string g_exe_dir = fs::parent(fs::parent(fs::executable_path()));
} // namespace

void PyUtilApi::add_util_api(PyApi::PyModule & pymodule)
{
    auto m_sihd = pymodule.module();

    m_sihd.attr("version") = SIHD_VERSION_STRING;
    m_sihd.attr("version_num") = SIHD_VERSION_NUM;
    m_sihd.attr("version_major") = SIHD_VERSION_MAJOR;
    m_sihd.attr("version_minor") = SIHD_VERSION_MINOR;
    m_sihd.attr("version_patch") = SIHD_VERSION_PATCH;

    pybind11::module m_util = m_sihd.def_submodule("util", "sihd::util");

    m_util.def_submodule("log", "sihd::util::Logger")
        .def(
            "emergency",
            +[](std::string_view log) { g_py_logger.log(LogLevel::emergency, log); },
            pybind11::call_guard<pybind11::gil_scoped_release>())
        .def(
            "alert",
            +[](std::string_view log) { g_py_logger.log(LogLevel::alert, log); },
            pybind11::call_guard<pybind11::gil_scoped_release>())
        .def(
            "critical",
            +[](std::string_view log) { g_py_logger.log(LogLevel::critical, log); },
            pybind11::call_guard<pybind11::gil_scoped_release>())
        .def(
            "error",
            +[](std::string_view log) { g_py_logger.log(LogLevel::error, log); },
            pybind11::call_guard<pybind11::gil_scoped_release>())
        .def(
            "warning",
            +[](std::string_view log) { g_py_logger.log(LogLevel::warning, log); },
            pybind11::call_guard<pybind11::gil_scoped_release>())
        .def(
            "notice",
            +[](std::string_view log) { g_py_logger.log(LogLevel::notice, log); },
            pybind11::call_guard<pybind11::gil_scoped_release>())
        .def(
            "info",
            +[](std::string_view log) { g_py_logger.log(LogLevel::info, log); },
            pybind11::call_guard<pybind11::gil_scoped_release>())
        .def(
            "debug",
            +[](std::string_view log) { g_py_logger.log(LogLevel::debug, log); },
            pybind11::call_guard<pybind11::gil_scoped_release>());

    m_util.def_submodule("types", "sihd::util::Types")
        .def("type_size", &type::size)
        .def("type_str", &type::str)
        .def("from_str", &type::from_str);

    m_util.def_submodule("thread", "sihd::util::Thread")
        .def("id", &thread::id)
        .def("id_str", static_cast<std::string (*)(pthread_t)>(&thread::id_str))
        .def("set_name", &thread::set_name)
        .def("name", &thread::name);

    m_util.def_submodule("path", "sihd::util::Path")
        .def("set", &path::set)
        .def("clear", &path::clear)
        .def("clear_all", &path::clear_all)
        .def("get", static_cast<std::string (*)(const std::string &)>(&path::get))
        .def("get_from_url",
             static_cast<std::string (*)(const std::string &, const std::string &)>(&path::get))
        .def("get_from_path", &path::get_from)
        .def("find", &path::find);

    m_util
        .def_submodule("time", "sihd::util::time")
        // sleep
        .def("nsleep", &time::nsleep, pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("usleep", &time::usleep, pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("msleep", &time::msleep, pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("sleep", &time::sleep, pybind11::call_guard<pybind11::gil_scoped_release>())
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

    pybind11::class_<Timestamp>(m_util, "Timestamp").def(pybind11::init<time_t>());

    pybind11::class_<Splitter>(m_util, "Splitter")
        .def(pybind11::init<>())
        .def("set_empty_delimitations", &Splitter::set_empty_delimitations)
        .def("set_delimiter", &Splitter::set_delimiter)
        .def("set_delimiter_spaces", &Splitter::set_delimiter_spaces)
        .def("set_open_escape_sequences", &Splitter::set_open_escape_sequences)
        .def("set_escape_sequences_all", &Splitter::set_escape_sequences_all)
        .def("split", &Splitter::split)
        .def("count_tokens", &Splitter::count_tokens);

    pybind11::class_<Configurable>(m_util, "Configurable")
        .def("set_conf", &PyUtilApi::_configurable_set_conf);

    pybind11::class_<Named, SmartNodePtr<Named>>(m_util, "Named")
        // keep_alive 1 -> this | 2 -> first arg | 3 -> parent
        // keeps alive parent while named object is alive
        .def(pybind11::init<const std::string &, Node *>(), pybind11::keep_alive<1, 3>())
        .def(pybind11::init<const std::string &>())
        .def_property_readonly(
            "c_ptr",
            +[](const Named *self) -> int64_t { return (int64_t)self; })
        .def("parent",
             static_cast<Node *(Named::*)()>(&Named::parent),
             pybind11::return_value_policy::reference_internal)
        .def("name", &Named::name)
        .def("full_name", &Named::full_name)
        .def("class_name", &Named::class_name)
        .def("description", &Named::description)
        .def("full_name", &Named::full_name)
        .def("root", &Named::root, pybind11::return_value_policy::reference_internal)
        .def("find", static_cast<Named *(Named::*)(const std::string &)>(&Named::find));

    pybind11::class_<Node::ChildEntry>(m_util, "ChildEntry")
        .def_readonly("name", &Node::ChildEntry::name)
        .def_readonly("obj", &Node::ChildEntry::obj)
        .def_readonly("ownership", &Node::ChildEntry::ownership);

    pybind11::class_<Node, Named, SmartNodePtr<Node>>(m_util, "Node")
        // keeps alive parent while node object is alive
        .def(pybind11::init<const std::string &, Node *>(), pybind11::keep_alive<1, 3>())
        .def(pybind11::init<const std::string &>())
        .def("get_child",
             static_cast<Named *(Node::*)(const std::string &)>(&Node::get_child),
             pybind11::return_value_policy::reference_internal)
        .def(
            "add_child",
            +[](Node *self, Named *child) { return self->add_child(child); })
        .def(
            "add_child_name",
            +[](Node *self, const std::string & name, Named *child) { return self->add_child(name, child); })
        .def("remove_child", static_cast<bool (Node::*)(const Named *)>(&Node::remove_child))
        .def("remove_child_name", static_cast<bool (Node::*)(const std::string &)>(&Node::remove_child))
        .def("is_link", &Node::is_link)
        .def("add_link", &Node::add_link)
        .def("remove_link", &Node::remove_link)
        .def("resolve_link", &Node::resolve_link, pybind11::return_value_policy::reference_internal)
        .def("resolve_links", &Node::resolve_links)
        .def("tree_str", static_cast<std::string (Node::*)() const>(&Node::tree_str))
        .def("tree_desc_str", &Node::tree_desc_str)
        // ties lifetime of return to 'this'
        .def("children", &Node::children, pybind11::return_value_policy::reference_internal)
        .def("children_keys", &Node::children_keys, pybind11::return_value_policy::reference_internal);

    pybind11::class_<ServiceController>(m_util, "ServiceController").def("state", &ServiceController::state);

    pybind11::class_<AService>(m_util, "AService")
        .def("setup", &AService::setup, pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("init", &AService::init, pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("start", &AService::start, pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("stop", &AService::stop, pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("reset", &AService::reset, pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("is_running", &AService::is_running)
        .def("service_ctrl", &AService::service_ctrl, pybind11::return_value_policy::reference_internal);

    pybind11::class_<IClock>(m_util, "IClock")
        .def("is_steady", &IClock::is_steady)
        .def("start", &IClock::start)
        .def("stop", &IClock::stop)
        .def("now", &IClock::now);

    pybind11::class_<SystemClock, IClock>(m_util, "SystemClock").def(pybind11::init<>());

    pybind11::class_<SteadyClock, IClock>(m_util, "SteadyClock").def(pybind11::init<>());

    m_util.attr("clock") = &Clock::default_clock;

    pybind11::class_<Waitable>(m_util, "Waitable")
        .def(pybind11::init<>())
        .def("notify", &Waitable::notify)
        .def("notify_all", &Waitable::notify_all);
    // .def("wait", &Waitable::wait, pybind11::call_guard<pybind11::gil_scoped_release>())
    // .def("wait_elapsed",
    //      &Waitable::wait_elapsed,
    //      pybind11::call_guard<pybind11::gil_scoped_release>())
    // .def("wait_until", &Waitable::wait_until, pybind11::call_guard<pybind11::gil_scoped_release>())
    // .def("wait_for", &Waitable::wait_for, pybind11::call_guard<pybind11::gil_scoped_release>())
    // .def("wait_for_elapsed", &Waitable::wait_for_elapsed,
    // pybind11::call_guard<pybind11::gil_scoped_release>());

    pybind11::class_<Scheduler, Named, Configurable, SmartNodePtr<Scheduler>>(m_util, "Scheduler")
        .def(pybind11::init<const std::string &, Node *>(), pybind11::keep_alive<1, 3>())
        .def(pybind11::init<const std::string &>())
        .def("clock", &Scheduler::clock, pybind11::return_value_policy::reference_internal)
        .def("set_clock", &Scheduler::set_clock)
        .def("start", &Scheduler::start, pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("stop", &Scheduler::stop, pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("is_running", &Scheduler::is_running)
        .def("pause", &Scheduler::pause)
        .def("resume", &Scheduler::resume, pybind11::call_guard<pybind11::gil_scoped_release>())
        .def(
            "add_task",
            +[](Scheduler & self,
                const pybind11::function & task,
                time_t run_at,
                time_t run_in,
                time_t reschedule_time) {
                self.add_task(new PyUtilApi::PyTask(task,
                                                    util::TaskOptions {
                                                        .run_at = run_at,
                                                        .run_in = run_in,
                                                        .reschedule_time = reschedule_time,
                                                    }));
            },
            pybind11::arg("task"),
            pybind11::arg("run_at") = 0,
            pybind11::arg("run_in") = 0,
            pybind11::arg("reschedule_time") = 0)
        .def("clear_tasks", &Scheduler::clear_tasks, pybind11::call_guard<pybind11::gil_scoped_release>())
        .def_readonly("overruns", &Scheduler::overruns)
        .def_readwrite("overrun_at", &Scheduler::overrun_at)
        .def_readwrite("acceptable_task_preplay_ns_time", &Scheduler::acceptable_task_preplay_ns_time);

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
        .def("data_type_str", &IArray::data_type_str)
        .def("hexdump", &IArray::hexdump)
        .def("str", static_cast<std::string (IArray::*)() const>(&IArray::str))
        .def("str", static_cast<std::string (IArray::*)(char) const>(&IArray::str))
        .def("clear", &IArray::clear)
        .def("__len__", &IArray::size);
    pybind11::class_<ArrChar, IArray>(m_util, "ArrChar")
        .def(pybind11::init<>())
        .def(pybind11::init<size_t>())
        .def(pybind11::init<const std::vector<char>>())
        .def(pybind11::init<std::string_view>())
        .def("clone", &ArrChar::clone)
        .def("push_back", static_cast<bool (ArrChar::*)(std::string_view)>(&ArrChar::push_back))
        .def("push_back", &PyUtilApi::_array_py_push_back_list<char>)
        .def("push_back", &PyUtilApi::_array_py_push_back_tuple<char>)
        .def("push_front", static_cast<bool (ArrChar::*)(std::string_view)>(&ArrChar::push_front))
        .def("push_front", &PyUtilApi::_array_py_push_front_list<char>)
        .def("push_front", &PyUtilApi::_array_py_push_front_tuple<char>)
        .def("pop", &ArrChar::pop)
        .def("front", &ArrChar::front)
        .def("back", &ArrChar::back)
        .def("at", &ArrChar::at)
        .def("__setitem__", &ArrChar::set)
        .def("__getitem__", &PyUtilApi::_array_py_getitem<char>)
        .def("__contains__", &PyUtilApi::_array_py_contains<char>)
        .def("__reversed__", &PyUtilApi::_array_py_reversed<char>)
        .def("__iter__", &PyUtilApi::_array_py_iter<char>, pybind11::keep_alive<0, 1>());
    DECLARE_ARRAY_USERTYPE(ArrBool, bool)
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
}

bool PyUtilApi::_configurable_set_conf(Configurable *self, const pybind11::kwargs & kwargs)
{
    std::string key;
    bool ret = true;
    for (const auto & item : kwargs)
    {
        try
        {
            key = pybind11::str(item.first);
        }
        catch (const pybind11::cast_error & e)
        {
            throw std::runtime_error("configuration key is not a string");
        }
        PyObject *val = item.second.ptr();
        if (PyTuple_Check(val))
        {
            pybind11::tuple tuple = item.second.cast<pybind11::tuple>();
            for (const auto & el : tuple)
            {
                ret = PyUtilApi::_configurable_set_single_conf(self, key, el) && ret;
            }
        }
        else if (PyList_Check(val))
        {
            pybind11::list list = item.second.cast<pybind11::list>();
            for (const auto & el : list)
            {
                ret = PyUtilApi::_configurable_set_single_conf(self, key, el) && ret;
            }
        }
        else
            ret = PyUtilApi::_configurable_set_single_conf(self, key, item.second) && ret;
    }
    return ret;
}

bool PyUtilApi::_configurable_set_single_conf(sihd::util::Configurable *self,
                                              const std::string & key,
                                              const pybind11::handle & handle)
{
    try
    {
        return self->set_conf_str(key, handle.cast<std::string>());
    }
    catch (const pybind11::cast_error & e)
    {
    }
    PyObject *val = handle.ptr();
    if (PyBool_Check(val))
        return self->set_conf<bool>(key, handle.cast<bool>());
    else if (PyLong_Check(val))
        return self->set_conf_int(key, handle.cast<int64_t>());
    else if (PyFloat_Check(val))
        return self->set_conf_float(key, handle.cast<double>());
    throw std::runtime_error(fmt::format("configuration '{}' type error", key));
}

static void __attribute__((constructor)) premain()
{
    PyApi::add_api("sihd_util", &PyUtilApi::add_util_api);
}

/* ************************************************************************* */
/* PyTask */
/* ************************************************************************* */

PyUtilApi::PyTask::PyTask(const pybind11::function & task, const sihd::util::TaskOptions & options):
    Task(nullptr, options),
    _fun(task)
{
}

PyUtilApi::PyTask::~PyTask() = default;

bool PyUtilApi::PyTask::run()
{
    pybind11::gil_scoped_acquire acquire;
    pybind11::object ret = _fun();
    return ret.cast<bool>();
}

} // namespace sihd::py

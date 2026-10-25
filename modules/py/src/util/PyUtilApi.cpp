#include <pybind11/pybind11.h>

#include <algorithm>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include <sihd/py/util/PyUtilApi.hpp>
#include <sihd/util/AService.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/IHandler.hpp>
#include <sihd/util/LoggerManager.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/Scheduler.hpp>
#include <sihd/util/ServiceController.hpp>
#include <sihd/util/Splitter.hpp>
#include <sihd/util/Waitable.hpp>
#include <sihd/util/thread.hpp>
#include <sihd/util/time.hpp>
#include <sihd/util/type.hpp>
#include <sihd/util/version.hpp>

#include "../expected_caster.hpp"

#define DECLARE_ARRAY_USERTYPE(ArrType, PrimitiveType)                                                                 \
    pybind11::class_<ArrType, IArray>(m_util, #ArrType)                                                                \
        .def(pybind11::init<>())                                                                                       \
        .def(pybind11::init<size_t>())                                                                                 \
        .def(pybind11::init<const std::vector<PrimitiveType>>())                                                       \
        .def("clone", &ArrType::clone)                                                                                 \
        .def("push_back", static_cast<bool (ArrType::*)(const PrimitiveType &)>(&ArrType::push_back))                  \
        .def("push_back", &array_py_push_back_list<PrimitiveType>)                                                     \
        .def("push_back", &array_py_push_back_tuple<PrimitiveType>)                                                    \
        .def("push_front", static_cast<bool (ArrType::*)(const PrimitiveType &)>(&ArrType::push_front))                \
        .def("push_front", &array_py_push_front_list<PrimitiveType>)                                                   \
        .def("push_front", &array_py_push_front_tuple<PrimitiveType>)                                                  \
        .def("pop", &ArrType::pop)                                                                                     \
        .def("front", &ArrType::front)                                                                                 \
        .def("back", &ArrType::back)                                                                                   \
        .def("at", &ArrType::at)                                                                                       \
        .def("__setitem__", &ArrType::set)                                                                             \
        .def("__getitem__", &array_py_getitem<PrimitiveType>)                                                          \
        .def("__contains__", &array_py_contains<PrimitiveType>)                                                        \
        .def("__reversed__", &array_py_reversed<PrimitiveType>)                                                        \
        .def("__iter__", &array_py_iter<PrimitiveType>, pybind11::keep_alive<0, 1>());

namespace sihd::py
{

using namespace sihd::util;
SIHD_NEW_LOGGER("sihd::py");

namespace
{
Logger g_py_logger("sihd::py");

// notify may fire on a service thread: take the GIL, and hold the callable via shared_ptr (GIL-taking deleter)
class PyServiceObserver: public IHandler<ServiceController *>
{
    public:
        explicit PyServiceObserver(pybind11::function fun):
            _fun(std::shared_ptr<pybind11::function>(new pybind11::function(std::move(fun)), [](pybind11::function *p) {
                pybind11::gil_scoped_acquire acquire;
                delete p;
            }))
        {
        }

        void handle(ServiceController *ctrl) override
        {
            pybind11::gil_scoped_acquire acquire;
            (*_fun)(pybind11::cast(ctrl, pybind11::return_value_policy::reference));
        }

    private:
        std::shared_ptr<pybind11::function> _fun;
};

template <typename T>
bool array_py_push_back_list(Array<T> & self, pybind11::list list)
{
    bool ret = true;
    for (const auto & item : list)
    {
        if (self.push_back(item.cast<T>()) == false)
        {
            ret = false;
            break;
        }
    }
    return ret;
}

template <typename T>
bool array_py_push_back_tuple(Array<T> & self, pybind11::tuple tuple)
{
    bool ret = true;
    for (const auto & item : tuple)
    {
        if (self.push_back(item.cast<T>()) == false)
        {
            ret = false;
            break;
        }
    }
    return ret;
}

template <typename T>
bool array_py_push_front_list(Array<T> & self, pybind11::list list)
{
    bool ret = true;
    for (const auto & item : list)
    {
        if (self.push_front(item.cast<T>()) == false)
        {
            ret = false;
            break;
        }
    }
    return ret;
}

template <typename T>
bool array_py_push_front_tuple(Array<T> & self, pybind11::tuple tuple)
{
    bool ret = true;
    for (const auto & item : tuple)
    {
        if (self.push_front(item.cast<T>()) == false)
        {
            ret = false;
            break;
        }
    }
    return ret;
}

template <typename T>
T array_py_getitem(const Array<T> & self, size_t idx)
{
    if (idx >= self.size())
        throw pybind11::index_error();
    return self.at(idx);
}

template <typename T>
pybind11::iterator array_py_iter(const Array<T> & self)
{
    return pybind11::make_iterator(self.cbegin(), self.cend());
}

template <typename T>
bool array_py_contains(const Array<T> & self, T value)
{
    return std::find(self.cbegin(), self.cend(), value) != self.cend();
}

template <typename T>
Array<T> array_py_reversed(const Array<T> & self)
{
    Array<T> ret;
    ret.resize(self.size());
    std::copy(self.crbegin(), self.crend(), ret.begin());
    return ret;
}

std::mutex g_service_obs_mutex;
std::multimap<ServiceController *, std::unique_ptr<PyServiceObserver>> g_service_observers;

void __attribute__((constructor)) premain()
{
    PyApi::add_api("sihd_util", &PyUtilApi::add_util_api);
}
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
            pybind11::call_guard<pybind11::gil_scoped_release>())
        .def(
            "console",
            +[]() { LoggerManager::console(); })
        .def(
            "stream",
            +[]() { LoggerManager::stream(); })
        .def("clear", +[]() { LoggerManager::clear_loggers(); });

    m_util.def_submodule("types", "sihd::util::Types")
        .def("type_size", &type::size)
        .def("type_str", static_cast<const char *(*)(Type)>(&type::str))
        .def("from_str", &type::from_str);

    m_util.def_submodule("thread", "sihd::util::Thread")
        .def("id", &thread::id)
        .def("id_str", static_cast<std::string (*)(pthread_t)>(&thread::id_str))
        .def(
            "set_name",
            +[](const std::string & name) { return thread::set_name(name); })
        .def("name", &thread::name);

    m_util.def_submodule("time", "sihd::util::time")
        .def("nsleep", &time::nsleep, pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("usleep", &time::usleep, pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("msleep", &time::msleep, pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("sleep", &time::sleep, pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("to_us", &time::to_micro)
        .def("to_ms", &time::to_milli)
        .def("to_sec", &time::to_sec)
        .def("to_min", &time::to_min)
        .def("to_hours", &time::to_hours)
        .def("to_days", &time::to_days)
        .def("to_double", &time::to_double)
        .def("to_hz", &time::to_freq)
        .def("us", &time::micro)
        .def("ms", &time::milli)
        .def("sec", &time::sec)
        .def("min", &time::min)
        .def("hours", &time::hours)
        .def("days", &time::days)
        .def("from_double", &time::from_double)
        .def("hz", &time::freq);

    pybind11::class_<Timestamp>(m_util, "Timestamp").def(pybind11::init<int64_t>());

    pybind11::class_<Duration>(m_util, "Duration")
        .def(pybind11::init<int64_t>())
        .def("str", &Duration::str, pybind11::arg("total_parenthesis") = false, pybind11::arg("nano") = false)
        .def("__int__", [](const Duration & self) { return static_cast<int64_t>(self); })
        .def("__str__", [](const Duration & self) { return self.str(); });
    // a plain int (ns) converts to Duration: wait_for(sihd.util.time.ms(500), 1)
    pybind11::implicitly_convertible<int64_t, Duration>();

    pybind11::class_<Splitter>(m_util, "Splitter")
        .def(pybind11::init<>())
        .def("set_empty_delimitations", &Splitter::set_empty_delimitations)
        .def("set_delimiter", &Splitter::set_delimiter)
        .def("set_delimiter_spaces", &Splitter::set_delimiter_spaces)
        .def("set_open_escape_sequences", &Splitter::set_open_escape_sequences)
        .def("set_escape_sequences_all", &Splitter::set_escape_sequences_all)
        .def("split", &Splitter::split)
        .def("count_tokens", &Splitter::count_tokens);

    pybind11::class_<Configurable>(m_util, "Configurable").def("set_conf", &PyUtilApi::_configurable_set_conf);

    pybind11::class_<Named, SmartNodePtr<Named>>(m_util, "Named")
        .def(pybind11::init([](const std::string & name, Node *parent) {
                 auto node = std::make_unique<Named>(name);
                 if (parent != nullptr)
                 {
                     auto added = parent->add_child(node.get(), false);
                     if (!added)
                         raise_error(added.error());
                 }
                 return node.release();
             }),
             pybind11::keep_alive<1, 3>())
        .def(pybind11::init<const std::string &>())
        .def_property_readonly(
            "c_ptr",
            +[](const Named *self) -> int64_t { return (int64_t)self; })
        .def(
            "parent",
            +[](Named *self) -> Node * { return self->parent(); },
            pybind11::return_value_policy::reference_internal)
        .def("name", &Named::name)
        .def("full_name", &Named::full_name)
        .def("class_name", &Named::class_name)
        .def("description", &Named::description)
        .def("full_name", &Named::full_name)
        .def(
            "root",
            +[](Named *self) -> Node * { return self->root(); },
            pybind11::return_value_policy::reference_internal)
        .def(
            "find",
            +[](Named *self, const std::string & path) -> Named * { return self->find(path); },
            pybind11::return_value_policy::reference_internal);

    pybind11::class_<Node::ChildEntry>(m_util, "ChildEntry")
        .def_readonly("name", &Node::ChildEntry::name)
        .def_readonly("obj", &Node::ChildEntry::obj)
        .def_readonly("ownership", &Node::ChildEntry::ownership);

    pybind11::class_<Node, Named, SmartNodePtr<Node>>(m_util, "Node")
        .def(pybind11::init([](const std::string & name, Node *parent) {
                 auto node = std::make_unique<Node>(name);
                 if (parent != nullptr)
                 {
                     auto added = parent->add_child(node.get(), false);
                     if (!added)
                         raise_error(added.error());
                 }
                 return node.release();
             }),
             pybind11::keep_alive<1, 3>())
        .def(pybind11::init<const std::string &>())
        .def(
            "get_child",
            +[](Node *self, const std::string & name) -> Named * { return self->get_child(name); },
            pybind11::return_value_policy::reference_internal)
        .def(
            "add_child",
            +[](Node *self, Named *child) { return self->add_child(child, false); })
        .def(
            "add_child_name",
            +[](Node *self, const std::string & name, Named *child) { return self->add_child(name, child, false); })
        .def("remove_child", static_cast<bool (Node::*)(const Named *)>(&Node::remove_child))
        .def("remove_child_name", static_cast<bool (Node::*)(const std::string &)>(&Node::remove_child))
        .def("is_link", &Node::is_link)
        .def("add_link", &Node::add_link)
        .def("remove_link", &Node::remove_link)
        .def("resolve_link", &Node::resolve_link, pybind11::return_value_policy::reference_internal)
        .def("resolve_links", [](Node *self) { return self->resolve_links(); })
        .def("tree_str", static_cast<std::string (Node::*)() const>(&Node::tree_str))
        .def("tree_desc_str", &Node::tree_desc_str)
        .def("children", &Node::children, pybind11::return_value_policy::reference_internal)
        .def("children_keys", &Node::children_keys, pybind11::return_value_policy::reference_internal);

    pybind11::enum_<ServiceController::State>(m_util, "ServiceState")
        .value("NoState", ServiceController::State::None)
        .value("Configuring", ServiceController::State::Configuring)
        .value("Configured", ServiceController::State::Configured)
        .value("Initializing", ServiceController::State::Initializing)
        .value("Starting", ServiceController::State::Starting)
        .value("Running", ServiceController::State::Running)
        .value("Stopping", ServiceController::State::Stopping)
        .value("Stopped", ServiceController::State::Stopped)
        .value("Resetting", ServiceController::State::Resetting)
        .value("StartFailure", ServiceController::State::StartFailure)
        .value("StopFailure", ServiceController::State::StopFailure)
        .value("Failure", ServiceController::State::Failure);

    pybind11::class_<ServiceController>(m_util, "ServiceController")
        .def("state", &ServiceController::state)
        .def("state_str", [](const ServiceController & self) { return ServiceController::state_str(self.state()); })
        .def("add_observer",
             [](ServiceController & self, pybind11::function fun) {
                 auto obs = std::make_unique<PyServiceObserver>(std::move(fun));
                 PyServiceObserver *ptr = obs.get();
                 {
                     std::lock_guard lock(g_service_obs_mutex);
                     g_service_observers.emplace(&self, std::move(obs));
                 }
                 pybind11::gil_scoped_release release;
                 self.add_observer(ptr);
             })
        .def("remove_observers", [](ServiceController & self) {
            std::vector<std::unique_ptr<PyServiceObserver>> removed;
            {
                std::lock_guard lock(g_service_obs_mutex);
                auto range = g_service_observers.equal_range(&self);
                for (auto it = range.first; it != range.second; ++it)
                    removed.push_back(std::move(it->second));
                g_service_observers.erase(&self);
            }
            pybind11::gil_scoped_release release;
            for (const auto & obs : removed)
                self.remove_observer(obs.get());
        });

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

    pybind11::enum_<LatenessPolicy>(m_util, "LatenessPolicy")
        .value("replay_missed", LatenessPolicy::replay_missed)
        .value("push_back", LatenessPolicy::push_back)
        .value("skip_missed", LatenessPolicy::skip_missed);

    pybind11::enum_<IdlePolicy>(m_util, "IdlePolicy")
        .value("sleep", IdlePolicy::sleep)
        .value("sleep_then_spin", IdlePolicy::sleep_then_spin);

    pybind11::class_<Scheduler, Named, Configurable, SmartNodePtr<Scheduler>>(m_util, "Scheduler")
        .def(pybind11::init<const std::string &, Node *>(), pybind11::keep_alive<1, 3>())
        .def(pybind11::init<const std::string &>())
        .def("clock", &Scheduler::clock, pybind11::return_value_policy::reference_internal)
        .def("set_clock", &Scheduler::set_clock)
        .def("set_no_delay", &Scheduler::set_no_delay, pybind11::arg("active"))
        .def("set_idle_policy", &Scheduler::set_idle_policy, pybind11::arg("policy"))
        .def("idle_policy", &Scheduler::idle_policy)
        .def("set_spin_window", &Scheduler::set_spin_window, pybind11::arg("window"))
        .def("spin_window", &Scheduler::spin_window)
        .def("set_skip_missed_on_start", &Scheduler::set_skip_missed_on_start, pybind11::arg("active"))
        .def("skip_missed_on_start", &Scheduler::skip_missed_on_start)
        .def("start",
             [](Scheduler & self) -> bool {
                 self.set_start_synchronised(true);
                 pybind11::gil_scoped_release release;
                 return self.start();
             })
        .def("stop", &Scheduler::stop, pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("is_running", &Scheduler::is_running)
        .def("pause", &Scheduler::pause)
        .def("resume", &Scheduler::resume, pybind11::call_guard<pybind11::gil_scoped_release>())
        .def(
            "add_task",
            +[](Scheduler & self,
                const pybind11::function & task,
                int64_t run_at,
                int64_t run_in,
                int64_t reschedule_time,
                LatenessPolicy late_policy) {
                self.add_task(new PyUtilApi::PyTask(task,
                                                    util::TaskOptions {
                                                        .run_at = Timestamp(run_at),
                                                        .run_in = Duration(run_in),
                                                        .reschedule_time = Duration(reschedule_time),
                                                        .late_policy = late_policy,
                                                    }));
            },
            pybind11::arg("task"),
            pybind11::arg("run_at") = 0,
            pybind11::arg("run_in") = 0,
            pybind11::arg("reschedule_time") = 0,
            pybind11::arg("late_policy") = LatenessPolicy::replay_missed)
        .def("clear_tasks", &Scheduler::clear_tasks)
        .def_property_readonly(
            "overruns",
            +[](const Scheduler & self) { return self.overruns.load(); })
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
        .def("push_back", &array_py_push_back_list<char>)
        .def("push_back", &array_py_push_back_tuple<char>)
        .def("push_front", static_cast<bool (ArrChar::*)(std::string_view)>(&ArrChar::push_front))
        .def("push_front", &array_py_push_front_list<char>)
        .def("push_front", &array_py_push_front_tuple<char>)
        .def("pop", &ArrChar::pop)
        .def("front", &ArrChar::front)
        .def("back", &ArrChar::back)
        .def("at", &ArrChar::at)
        .def("__setitem__", &ArrChar::set)
        .def("__getitem__", &array_py_getitem<char>)
        .def("__contains__", &array_py_contains<char>)
        .def("__reversed__", &array_py_reversed<char>)
        .def("__iter__", &array_py_iter<char>, pybind11::keep_alive<0, 1>());
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
        catch (const pybind11::cast_error &)
        {
            raise_error(Error(ErrorCode::invalid_argument, "set_conf keys must be strings"));
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

bool PyUtilApi::_configurable_set_single_conf(Configurable *self,
                                              const std::string & key,
                                              const pybind11::handle & handle)
{
    try
    {
        auto res = self->set_conf_str(key, handle.cast<std::string>());
        if (!res)
            raise_error(res.error());
        return true;
    }
    catch (const pybind11::cast_error &)
    {
    }
    PyObject *val = handle.ptr();
    if (PyBool_Check(val))
    {
        auto res = self->set_conf<bool>(key, handle.cast<bool>());
        if (!res)
            raise_error(res.error());
        return true;
    }
    if (PyLong_Check(val))
    {
        auto res = self->set_conf_int(key, handle.cast<int64_t>());
        if (!res)
            raise_error(res.error());
        return true;
    }
    if (PyFloat_Check(val))
    {
        auto res = self->set_conf_float(key, handle.cast<double>());
        if (!res)
            raise_error(res.error());
        return true;
    }
    raise_error(Error(ErrorCode::invalid_argument, "set_conf refused the key '{}'", key));
}

PyUtilApi::PyTask::PyTask(const pybind11::function & task, const TaskOptions & options):
    Task(nullptr, options),
    _fun(task)
{
}

PyUtilApi::PyTask::~PyTask()
{
    if (_fun.ptr() != nullptr && Py_IsInitialized())
    {
        pybind11::gil_scoped_acquire acquire;
        _fun.dec_ref();
        _fun.release();
    }
}

bool PyUtilApi::PyTask::run()
{
    pybind11::gil_scoped_acquire acquire;
    try
    {
        pybind11::object ret = _fun();
        if (ret.is_none())
            return true;
        return ret.cast<bool>();
    }
    catch (pybind11::error_already_set & e)
    {
        SIHD_LOG(error, "PyTask: {}", e.what());
    }
    catch (const std::exception & e)
    {
        SIHD_LOG(error, "PyTask: {}", e.what());
    }
    return false;
}

} // namespace sihd::py

#include <map>
#include <memory>
#include <regex>

#include <sihd/lua/util/LuaUtilApi.hpp>
#include <sihd/util/ABlockingService.hpp>
#include <sihd/util/AService.hpp>
#include <sihd/util/AThreadedService.hpp>
#include <sihd/util/Clocks.hpp>
#include <sihd/util/LoggerManager.hpp>
#include <sihd/util/Named.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/Splitter.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/build.hpp>
#include <sihd/util/endian.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/term.hpp>
#include <sihd/util/thread.hpp>
#include <sihd/util/time.hpp>
#include <sihd/util/type.hpp>

#define DECLARE_ARRAY_USERTYPE(ArrType, PrimitiveType)                                                                 \
    .deriveClass<ArrType, IArray>(#ArrType)                                                                            \
        .addStaticFunction("new", &LuaUtilApi::_array_lua_new<PrimitiveType>)                                          \
        .addConstructor<void (*)()>()                                                                                  \
        .addFunction("clone", &ArrType::clone)                                                                         \
        .addFunction("push_back", &LuaUtilApi::_array_lua_push_back<PrimitiveType>)                                    \
        .addFunction("push_front", &LuaUtilApi::_array_lua_push_front<PrimitiveType>)                                  \
        .addFunction("copy_from", &LuaUtilApi::_array_lua_copy_table<PrimitiveType>)                                   \
        .addFunction("from", &LuaUtilApi::_array_lua_from<PrimitiveType>)                                              \
        .addFunction("pop", &ArrType::pop)                                                                             \
        .addFunction("front", &ArrType::front)                                                                         \
        .addFunction("back", &ArrType::back)                                                                           \
        .addFunction("at", &ArrType::at)                                                                               \
        .addFunction("set", &ArrType::set)                                                                             \
        .addFunction("__len", &ArrType::size)                                                                          \
        .endClass()

namespace sihd::lua
{

using namespace sihd::util;
SIHD_LOGGER;

namespace
{
// logger used by lua code
Logger g_lua_logger("sihd::lua");

// Bridges an Observable<ServiceController> notification to a Lua callback.
// The notify may fire on a service thread, so run the Lua call through a
// LuaThreadRunner (own coroutine + universe GIL), exactly like a channel observer.
class LuaServiceObserver: public sihd::util::IHandler<sihd::util::ServiceController *>
{
    public:
        LuaServiceObserver(lua_State *state, luabridge::LuaRef fun): _runner(fun)
        {
            Vm vm(state);
            lua_State *thread = vm.new_luathread();
            if (thread != nullptr)
                _runner.new_lua_state(thread);
        }

        void handle(sihd::util::ServiceController *ctrl) override
        {
            _runner.call_lua_method_noret<sihd::util::ServiceController *>(ctrl);
        }

    private:
        LuaUtilApi::LuaThreadRunner _runner;
};

std::mutex g_service_obs_mutex;
std::multimap<sihd::util::ServiceController *, std::unique_ptr<LuaServiceObserver>> g_service_observers;
} // namespace

bool LuaUtilApi::_configurable_recursive_set(Configurable *obj, const std::string & key, luabridge::LuaRef ref)
{
    switch (ref.type())
    {
        case LUA_TBOOLEAN:
        {
            return obj->set_conf<bool>(key, static_cast<bool>(ref));
        }
        case LUA_TTABLE:
        {
            bool ret = true;
            for (const auto & key_pair : luabridge::pairs(ref))
                ret = LuaUtilApi::_configurable_recursive_set(obj, key, key_pair.second) && ret;
            return ret;
        }
        case LUA_TNUMBER:
        {
            try
            {
                return obj->set_conf_float(key, static_cast<double>(ref));
            }
            catch (const std::invalid_argument & e)
            {
                return obj->set_conf_int(key, static_cast<int64_t>(ref));
            }
            return false;
        }
        case LUA_TSTRING:
        {
            return obj->set_conf_str(key, std::string(ref));
        }
        default:
        {
            g_lua_logger.error(fmt::format("Configuration key '{}' type error", key));
        }
    }
    return false;
}

void LuaUtilApi::load_threading(Vm & vm)
{
    luabridge::getGlobalNamespace(vm.lua_state())
        .beginNamespace("sihd")
        .beginNamespace("util")
        /**
         * Scheduler
         */
        .deriveClass<LuaScheduler, Named>("Scheduler")
        .addConstructorFrom<SmartNodePtr<LuaScheduler>, void(const std::string &, Node *)>()
        // Configurable
        .addFunction("set_conf", &LuaUtilApi::configurable_set_conf<LuaScheduler>)
        // Scheduler
        .addFunction("is_running", static_cast<bool (LuaScheduler::*)() const>(&Scheduler::is_running))
        .addFunction("pause", static_cast<void (LuaScheduler::*)()>(&Scheduler::pause))
        .addFunction("resume", static_cast<void (LuaScheduler::*)()>(&Scheduler::resume))
        .addFunction("now", static_cast<Timestamp (LuaScheduler::*)() const>(&Scheduler::now))
        .addFunction("clock", static_cast<IClock *(LuaScheduler::*)() const>(&Scheduler::clock))
        .addFunction("set_clock", static_cast<void (LuaScheduler::*)(IClock *)>(&Scheduler::set_clock))
        .addFunction("set_no_delay", static_cast<bool (LuaScheduler::*)(bool)>(&Scheduler::set_no_delay))
        .addFunction("clear_tasks", static_cast<void (LuaScheduler::*)()>(&Scheduler::clear_tasks))
        .addProperty(
            "overruns",
            +[](const LuaScheduler *self) { return self->overruns; })
        .addProperty(
            "overrun_at",
            +[](const LuaScheduler *self) { return self->overrun_at; },
            +[](LuaScheduler *self, luabridge::LuaRef val) { self->overrun_at = sihd::lua::to_duration(val); })
        .addProperty(
            "acceptable_task_preplay_ns_time",
            +[](const LuaScheduler *self) { return self->acceptable_task_preplay_ns_time; },
            +[](LuaScheduler *self, luabridge::LuaRef val) {
                self->acceptable_task_preplay_ns_time = sihd::lua::to_duration(val);
            })
        // LuaScheduler
        .addFunction("start",
                     std::function<bool(LuaScheduler *, lua_State *)>(+[](LuaScheduler *self, lua_State *state) {
                         self->set_state(state);
                         return self->start();
                     }))
        .addFunction("stop",
                     std::function<bool(LuaScheduler *, lua_State *)>(+[](LuaScheduler *self, lua_State *state) {
                         LuaGilRelease release(state);
                         return self->stop();
                     }))
        .addFunction(
            "add_task",
            +[](LuaScheduler *self, luabridge::LuaRef tbl, lua_State *state) {
                if (tbl.isTable() == false)
                    luaL_error(state, "add_task argument must be a table");

                luabridge::LuaRef lua_fun = tbl["task"];
                if (lua_fun.isFunction() == false)
                    luaL_error(state, "add_task table at 'task' must contain a function");

                Timestamp timestamp_to_run_at = 0;
                luabridge::LuaRef run_at = tbl["run_at"];
                if (run_at.isNil() == false)
                    timestamp_to_run_at = sihd::lua::to_timestamp(run_at);

                Duration timestamp_to_run_in = 0;
                luabridge::LuaRef run_in = tbl["run_in"];
                if (run_in.isNil() == false)
                    timestamp_to_run_in = sihd::lua::to_duration(run_in);

                Duration reschedule_time = 0;
                luabridge::LuaRef reschedule = tbl["reschedule_time"];
                if (reschedule.isNil() == false)
                    reschedule_time = sihd::lua::to_duration(reschedule);

                LuaTask *task_ptr = new LuaTask(lua_fun,
                                                util::TaskOptions {.run_at = timestamp_to_run_at,
                                                                   .run_in = timestamp_to_run_in,
                                                                   .reschedule_time = reschedule_time});
                if (task_ptr != nullptr)
                {
                    self->add_lua_task(task_ptr);
                }
            })
        .endClass()
        /**
         * Waitable
         */
        .beginClass<Waitable>("Waitable")
        .addConstructor<void (*)()>()
        .addFunction("notify", &Waitable::notify)
        .addFunction("notify_all", &Waitable::notify_all)
        .addFunction(
            "wait",
            +[](Waitable *self, lua_State *state) {
                LuaGilRelease release(state);
                self->wait();
            })
        .addFunction(
            "wait_until",
            +[](Waitable *self, luabridge::LuaRef t, lua_State *state) {
                Timestamp timestamp = sihd::lua::to_timestamp(t);
                LuaGilRelease release(state);
                return self->wait_until(timestamp);
            })
        .addFunction(
            "wait_for",
            +[](Waitable *self, luabridge::LuaRef d, lua_State *state) {
                Duration duration = sihd::lua::to_duration(d);
                LuaGilRelease release(state);
                return self->wait_for(duration);
            })
        .addFunction(
            "wait_elapsed",
            +[](Waitable *self, lua_State *state) {
                LuaGilRelease release(state);
                return self->wait_elapsed();
            })
        .addFunction(
            "wait_until_elapsed",
            +[](Waitable *self, luabridge::LuaRef t, lua_State *state) {
                Timestamp timestamp = sihd::lua::to_timestamp(t);
                LuaGilRelease release(state);
                return self->wait_until_elapsed(timestamp);
            })
        .addFunction(
            "wait_for_elapsed",
            +[](Waitable *self, luabridge::LuaRef d, lua_State *state) {
                Duration duration = sihd::lua::to_duration(d);
                LuaGilRelease release(state);
                return self->wait_for_elapsed(duration);
            })
        .endClass()
        /**
         * Waitable
         */
        .beginClass<LuaWorker>("Worker")
        .addConstructor<void (*)(luabridge::LuaRef ref)>()
        .addFunction("start_worker",
                     std::function<bool(LuaWorker *, const std::string &, lua_State *)>(
                         [](LuaWorker *self, const std::string & name, lua_State *state) {
                             self->set_state(state);
                             return self->start_worker(name);
                         }))
        .addFunction("start_sync_worker",
                     std::function<bool(LuaWorker *, const std::string &, lua_State *)>(
                         [](LuaWorker *self, const std::string & name, lua_State *state) {
                             self->set_state(state);
                             LuaGilRelease release(state);
                             return self->start_sync_worker(name);
                         }))
        .addFunction(
            "stop_worker",
            +[](LuaWorker *self, lua_State *state) {
                LuaGilRelease release(state);
                return self->stop_worker();
            })
        .addFunction("is_worker_running", static_cast<bool (LuaWorker::*)() const>(&Worker::is_worker_running))
        .addFunction("is_worker_started", static_cast<bool (LuaWorker::*)() const>(&Worker::is_worker_started))
        .endClass()
        .beginClass<LuaStepWorker>("StepWorker")
        .addConstructor<void (*)(luabridge::LuaRef ref)>()
        .addFunction("set_frequency", static_cast<bool (LuaStepWorker::*)(double)>(&StepWorker::set_frequency))
        .addFunction("start_worker",
                     std::function<bool(LuaStepWorker *, const std::string &, lua_State *)>(
                         [](LuaStepWorker *self, const std::string & name, lua_State *state) {
                             self->set_state(state);
                             return self->start_worker(name);
                         }))
        .addFunction("start_sync_worker",
                     std::function<bool(LuaStepWorker *, const std::string &, lua_State *)>(
                         [](LuaStepWorker *self, const std::string & name, lua_State *state) {
                             self->set_state(state);
                             LuaGilRelease release(state);
                             return self->start_sync_worker(name);
                         }))
        .addFunction(
            "stop_worker",
            +[](LuaStepWorker *self, lua_State *state) {
                LuaGilRelease release(state);
                return self->stop_worker();
            })
        .addFunction("is_worker_running", static_cast<bool (LuaStepWorker::*)() const>(&Worker::is_worker_running))
        .addFunction("is_worker_started", static_cast<bool (LuaStepWorker::*)() const>(&Worker::is_worker_started))
        .addFunction(
            "pause_worker",
            +[](LuaStepWorker *self, lua_State *state) {
                LuaGilRelease release(state);
                self->pause_worker();
            })
        .addFunction(
            "resume_worker",
            +[](LuaStepWorker *self, lua_State *state) {
                LuaGilRelease release(state);
                self->resume_worker();
            })
        .addFunction("nano_sleep_time", static_cast<Duration (LuaStepWorker::*)() const>(&StepWorker::nano_sleep_time))
        .addFunction("frequency", static_cast<double (LuaStepWorker::*)() const>(&StepWorker::frequency))
        .endClass()
        .endNamespace()
        .endNamespace();
}

void LuaUtilApi::load_tools(Vm & vm)
{
    luabridge::getGlobalNamespace(vm.lua_state())
        .beginNamespace("sihd")
        .beginNamespace("util")
        /**
         * Clock
         */
        .beginClass<IClock>("IClock")
        .addFunction("now", &IClock::now)
        .addFunction("is_steady", &IClock::is_steady)
        .endClass()
        .deriveClass<SystemClock, IClock>("SystemClock")
        .endClass()
        .deriveClass<SteadyClock, IClock>("SteadyClock")
        .endClass()
        .addVariable("clock", &Clock::default_clock)
        /**
         * Properties
         */
        .addProperty(
            "platform",
            +[] { return __SIHD_PLATFORM__; })
        /**
         * Splitter
         */
        .beginClass<Splitter>("Splitter")
        .addConstructor<void (*)()>()
        .addFunction("set_empty_delimitations", &Splitter::set_empty_delimitations)
        .addFunction("set_delimiter", &Splitter::set_delimiter)
        .addFunction("set_delimiter_spaces", &Splitter::set_delimiter_spaces)
        .addFunction("set_open_escape_sequences", &Splitter::set_open_escape_sequences)
        .addFunction("set_escape_sequences_all", &Splitter::set_escape_sequences_all)
        .addFunction("split", &Splitter::split)
        .addFunction("count_tokens", &Splitter::count_tokens)
        .endClass()
        /**
         * Timestamp
         */
        .beginClass<Timestamp>("Timestamp")
        .addConstructor<void (*)(int64_t)>()
        .addFunction("get", &Timestamp::get)
        .addFunction("in_interval", &Timestamp::in_interval)
        .addFunction("nanoseconds", &Timestamp::nanoseconds)
        .addFunction("microseconds", &Timestamp::microseconds)
        .addFunction("milliseconds", &Timestamp::milliseconds)
        .addFunction("seconds", &Timestamp::seconds)
        .addFunction("minutes", &Timestamp::minutes)
        .addFunction("hours", &Timestamp::hours)
        .addFunction("days", &Timestamp::days)
        .addFunction("timeoffset_str", &Timestamp::timeoffset_str)
        .addFunction("localtimeoffset_str", &Timestamp::localtimeoffset_str)
        .addFunction("format", static_cast<std::string (Timestamp::*)(std::string_view) const>(&Timestamp::format))
        .addFunction("local_format",
                     static_cast<std::string (Timestamp::*)(std::string_view) const>(&Timestamp::local_format))
        .addFunction("str", &Timestamp::str)
        .addFunction("local_str", &Timestamp::local_str)
        .addFunction("zone_str", &Timestamp::zone_str)
        .addFunction("sec_str", &Timestamp::sec_str)
        .addFunction("local_sec_str", &Timestamp::local_sec_str)
        .addFunction("day_str", &Timestamp::day_str)
        .addFunction("local_day_str", &Timestamp::local_day_str)
        .addFunction("is_leap_year", &Timestamp::is_leap_year)
        .addFunction("floor_day", &Timestamp::floor_day)
        .addFunction("modulo_min", &Timestamp::modulo_min)
        .addFunction(
            "__eq",
            +[](Timestamp self, Timestamp other) -> bool { return self == other; })
        .addFunction(
            "__lt",
            +[](Timestamp self, Timestamp other) -> bool { return self < other; })
        .addFunction(
            "__le",
            +[](Timestamp self, Timestamp other) -> bool { return self <= other; })
        .addFunction(
            "__add",
            +[](Timestamp self, Duration other) -> Timestamp { return self + other; })
        .addFunction(
            "__sub",
            +[](Timestamp self, Timestamp other) -> Duration { return self - other; })
        .endClass()
        /**
         * Duration
         */
        .beginClass<Duration>("Duration")
        .addConstructor<void (*)(int64_t)>()
        .addFunction("get", &Duration::get)
        .addFunction("nanoseconds", &Duration::nanoseconds)
        .addFunction("microseconds", &Duration::microseconds)
        .addFunction("milliseconds", &Duration::milliseconds)
        .addFunction("seconds", &Duration::seconds)
        .addFunction("minutes", &Duration::minutes)
        .addFunction("hours", &Duration::hours)
        .addFunction("days", &Duration::days)
        .addFunction("str", &Duration::str)
        .addFunction(
            "__eq",
            +[](Duration self, Duration other) -> bool { return self == other; })
        .addFunction(
            "__lt",
            +[](Duration self, Duration other) -> bool { return self < other; })
        .addFunction(
            "__le",
            +[](Duration self, Duration other) -> bool { return self <= other; })
        .addFunction(
            "__add",
            +[](Duration self, Duration other) -> Duration { return self + other; })
        .addFunction(
            "__sub",
            +[](Duration self, Duration other) -> Duration { return self - other; })
        .endClass()
        /**
         * Namespaces
         */
        .beginNamespace("time")
        // sleep
        .addFunction(
            "nsleep",
            +[](time::UnixTime t, lua_State *state) {
                LuaGilRelease release(state);
                return time::nsleep(t);
            })
        .addFunction(
            "usleep",
            +[](time::UnixTime t, lua_State *state) {
                LuaGilRelease release(state);
                return time::usleep(t);
            })
        .addFunction(
            "msleep",
            +[](time::UnixTime t, lua_State *state) {
                LuaGilRelease release(state);
                return time::msleep(t);
            })
        .addFunction(
            "sleep",
            +[](time::UnixTime t, lua_State *state) {
                LuaGilRelease release(state);
                return time::sleep(t);
            })
        // convert from nano
        .addFunction("to_us", &time::to_micro)
        .addFunction("to_ms", &time::to_milli)
        .addFunction("to_sec", &time::to_sec)
        .addFunction("to_min", &time::to_min)
        .addFunction("to_hours", &time::to_hours)
        .addFunction("to_days", &time::to_days)
        .addFunction("to_double", &time::to_double)
        .addFunction("to_hz", &time::to_freq)
        // convert to nano
        .addFunction("us", &time::micro)
        .addFunction("ms", &time::milli)
        .addFunction("sec", &time::sec)
        .addFunction("min", &time::min)
        .addFunction("hours", &time::hours)
        .addFunction("days", &time::days)
        .addFunction("from_double", &time::from_double)
        .addFunction("hz", &time::freq)
        .endNamespace() // time
        .beginNamespace("thread")
        .addFunction("id", &thread::id)
        .addFunction("id_str", thread::id_str)
        .addFunction("set_name", &thread::set_name)
        .addFunction("name", &thread::name)
        .endNamespace() // thread
        .beginNamespace("str")
        .addFunction("timeoffset_str", &str::timeoffset_str)
        .addFunction("localtimeoffset_str", &str::localtimeoffset_str)
        .addFunction("trim", &str::trim)
        .addFunction("replace", &str::replace)
        .addFunction("starts_with", &str::starts_with)
        .addFunction("ends_with", &str::ends_with)
        .addFunction(
            "split",
            +[](const std::string & str, const std::string & delimiter) {
                Splitter splitter(delimiter);
                return splitter.split(str);
            })
        .endNamespace() // str
        // path namespace removed - use PathManager class instead
        .beginNamespace("term")
        .addFunction("is_interactive", &term::is_interactive)
        .endNamespace() // term
        .beginNamespace("endian")
        .addFunction(
            "is_big",
            +[] { return std::endian::native == std::endian::big; })
        .addFunction(
            "is_little",
            +[] { return std::endian::native == std::endian::little; })
        .endNamespace()  // edian
        .endNamespace()  // util
        .endNamespace(); // sihd
}

void LuaUtilApi::load_base(Vm & vm)
{
    if (vm.refs_exists({"sihd", "version"}))
        return;
    luabridge::getGlobalNamespace(vm.lua_state())
        .beginNamespace("sihd")
        .addProperty(
            "version",
            +[]() { return SIHD_VERSION_STRING; })
        .addProperty(
            "version_num",
            +[]() { return SIHD_VERSION_NUM; })
        .addProperty(
            "version_major",
            +[]() { return SIHD_VERSION_MAJOR; })
        .addProperty(
            "version_minor",
            +[]() { return SIHD_VERSION_MINOR; })
        .addProperty(
            "version_patch",
            +[]() { return SIHD_VERSION_PATCH; })
        .beginNamespace("util")
        .beginNamespace("log")
        .addFunction(
            "emergency",
            +[](const std::string & log) { g_lua_logger.log(LogLevel::emergency, log); })
        .addFunction(
            "alert",
            +[](const std::string & log) { g_lua_logger.log(LogLevel::alert, log); })
        .addFunction(
            "critical",
            +[](const std::string & log) { g_lua_logger.log(LogLevel::critical, log); })
        .addFunction(
            "error",
            +[](const std::string & log) { g_lua_logger.log(LogLevel::error, log); })
        .addFunction(
            "warning",
            +[](const std::string & log) { g_lua_logger.log(LogLevel::warning, log); })
        .addFunction(
            "notice",
            +[](const std::string & log) { g_lua_logger.log(LogLevel::notice, log); })
        .addFunction(
            "info",
            +[](const std::string & log) { g_lua_logger.log(LogLevel::info, log); })
        .addFunction(
            "debug",
            +[](const std::string & log) { g_lua_logger.log(LogLevel::debug, log); })
        // sink configuration - wire SIHD's logger output from a config script
        .addFunction("console", +[]() { sihd::util::LoggerManager::console(); })
        .addFunction("stream", +[]() { sihd::util::LoggerManager::stream(); })
        .addFunction("clear", +[]() { sihd::util::LoggerManager::clear_loggers(); })
        .endNamespace()
        /**
         * Nodes
         */
        .beginClass<Node::ChildEntry>("ChildEntry")
        .addProperty("name", &Node::ChildEntry::name)
        .addProperty("obj", &Node::ChildEntry::obj)
        .addProperty("ownership", &Node::ChildEntry::ownership)
        .endClass()
        .beginClass<Named>("Named")
        .addFactory(
            +[](lua_State *L) -> Named * {
                std::string name = *luabridge::Stack<std::string>::get(L, 2);
                Node *parent = nullptr;
                if (!lua_isnoneornil(L, 3))
                    parent = *luabridge::Stack<Node *>::get(L, 3);
                Named *node = new Named(name);
                if (parent != nullptr)
                    parent->add_child(node, false);
                return node;
            },
            +[](Named *node) { delete node; })
        .addProperty(
            "c_ptr",
            +[](const Named *self) -> int64_t { return (int64_t)self; })
        .addFunction("parent", static_cast<Node *(Named::*)()>(&Named::parent))
        .addFunction("name", &Named::name)
        .addFunction("full_name", &Named::full_name)
        .addFunction("class_name", &Named::class_name)
        .addFunction("description", &Named::description)
        .addFunction("full_name", &Named::full_name)
        .addFunction("root", &Named::root)
        .addFunction("find", static_cast<Named *(Named::*)(const std::string &)>(&Named::find))
        .addFunction(
            "__eq",
            +[](const Named *self, const Named *other) -> bool { return self == other; })
        .addIndexMetaMethod([](Named & self, const luabridge::LuaRef & key, lua_State *L) {
            Named *res = self.find(key.tostring());
            if (res == nullptr)
                return luabridge::LuaRef(L, luabridge::LuaNil()); // or luaL_error("Failed lookup of key !")
            return luabridge::LuaRef(L, res);
        })
        .endClass()
        .deriveClass<Node, Named>("Node")
        .addFactory(
            +[](lua_State *L) -> Node * {
                std::string name = *luabridge::Stack<std::string>::get(L, 2);
                Node *parent = nullptr;
                if (!lua_isnoneornil(L, 3))
                    parent = *luabridge::Stack<Node *>::get(L, 3);
                Node *node = new Node(name);
                if (parent != nullptr)
                    parent->add_child(node, false);
                return node;
            },
            +[](Node *node) { delete node; })
        .addFunction("get_child", static_cast<Named *(Node::*)(const std::string &)>(&Node::get_child))
        .addFunction(
            "add_child",
            +[](Node *self, Named *child) { return self->add_child(child, false); })
        .addFunction(
            "add_child_name",
            +[](Node *self, const std::string & name, Named *child) { return self->add_child(name, child, false); })
        .addFunction("remove_child", static_cast<bool (Node::*)(const Named *)>(&Node::remove_child))
        .addFunction("remove_child_name", static_cast<bool (Node::*)(const std::string &)>(&Node::remove_child))
        .addFunction("is_link", &Node::is_link)
        .addFunction("add_link", &Node::add_link)
        .addFunction("remove_link", &Node::remove_link)
        .addFunction("resolve_link", &Node::resolve_link)
        .addFunction("resolve_links", &Node::resolve_links)
        .addFunction("tree_str", static_cast<std::string (Node::*)() const>(&Node::tree_str))
        .addFunction("tree_desc_str", &Node::tree_desc_str)
        .addFunction("children", &Node::children)
        .addFunction("children_keys", &Node::children_keys)
        .addFunction(
            "__eq",
            +[](const Node *self, const Named *other) -> bool { return self == other; })
        .endClass()
        /**
         * Array
         */
        // .beginNamespace("Type")
        // .addVariable("NONE", Type::TYPE_NONE)
        // .addVariable("BOOL", Type::TYPE_BOOL)
        // .addVariable("CHAR", Type::TYPE_CHAR)
        // .addVariable("BYTE", Type::TYPE_BYTE)
        // .addVariable("UBYTE", Type::TYPE_UBYTE)
        // .addVariable("SHORT", Type::TYPE_SHORT)
        // .addVariable("USHORT", Type::TYPE_USHORT)
        // .addVariable("INT", Type::TYPE_INT)
        // .addVariable("UINT", Type::TYPE_UINT)
        // .addVariable("LONG", Type::TYPE_LONG)
        // .addVariable("ULONG", Type::TYPE_ULONG)
        // .addVariable("FLOAT", Type::TYPE_FLOAT)
        // .addVariable("DOUBLE", Type::TYPE_DOUBLE)
        // .addVariable("OBJECT", Type::TYPE_OBJECT)
        // .endNamespace()
        .beginNamespace("types")
        .addFunction("type_size", static_cast<size_t (*)(Type)>(&type::size))
        .addFunction("type_str", static_cast<const char *(*)(Type)>(&type::str))
        .addFunction("from_str", static_cast<Type (*)(std::string_view)>(&type::from_str))
        .endNamespace()
        .beginClass<IArray>("IArray")
        .addFunction("size", &IArray::size)
        .addFunction("capacity", &IArray::capacity)
        .addFunction("data_size", &IArray::data_size)
        .addFunction("byte_size", &IArray::byte_size)
        .addFunction("resize", &IArray::resize)
        .addFunction("reserve", &IArray::reserve)
        .addFunction("byte_resize", &IArray::byte_resize)
        .addFunction("byte_reserve", &IArray::byte_reserve)
        .addFunction("data_type", &IArray::data_type)
        .addFunction("data_type_str", &IArray::data_type_str)
        .addFunction("hexdump", &IArray::hexdump)
        .addFunction(
            "str",
            +[](IArray *self, luabridge::LuaRef ref) {
                if (ref.isNil())
                    return self->str();
                return self->str(static_cast<char>(ref));
            })
        .addFunction("clear", &IArray::clear)
        // .addFunction("is_same_type", static_cast<bool (IArray::*)(const IArray &)
        // const>(&IArray::is_same_type))
        .endClass() DECLARE_ARRAY_USERTYPE(ArrBool, bool) DECLARE_ARRAY_USERTYPE(ArrChar, char)
            DECLARE_ARRAY_USERTYPE(ArrByte, int8_t) DECLARE_ARRAY_USERTYPE(ArrUByte, uint8_t)
                DECLARE_ARRAY_USERTYPE(ArrShort, int16_t) DECLARE_ARRAY_USERTYPE(ArrUShort, uint16_t)
                    DECLARE_ARRAY_USERTYPE(ArrInt, int32_t) DECLARE_ARRAY_USERTYPE(ArrUInt, uint32_t)
                        DECLARE_ARRAY_USERTYPE(ArrLong, int64_t) DECLARE_ARRAY_USERTYPE(ArrULong, uint64_t)
                            DECLARE_ARRAY_USERTYPE(ArrFloat, float) DECLARE_ARRAY_USERTYPE(ArrDouble, double)
        /**
         * Service
         */
        .beginClass<AService>("AService")
        .addFunction("setup", &AService::setup)
        .addFunction("init", &AService::init)
        .addFunction("start", &AService::start)
        .addFunction("stop", &AService::stop)
        .addFunction("reset", &AService::reset)
        .addFunction("is_running", &AService::is_running)
        .addFunction("service_ctrl",
                     +[](AService *self) -> ServiceController * {
                         return dynamic_cast<ServiceController *>(self->service_ctrl());
                     })
        .endClass()
        .beginClass<ServiceController>("ServiceController")
        .addFunction("state", &ServiceController::state)
        .addFunction("state_str", +[](ServiceController *self) -> std::string {
            return ServiceController::state_str(self->state());
        })
        // observe service state transitions from a config script
        .addFunction("add_observer",
                     +[](ServiceController *self, luabridge::LuaRef fun, lua_State *state) {
                         if (fun.isFunction() == false)
                         {
                             luaL_error(state, "add_observer: expected a function");
                             return;
                         }
                         auto obs = std::make_unique<LuaServiceObserver>(state, fun);
                         LuaServiceObserver *ptr = obs.get();
                         {
                             std::lock_guard l(g_service_obs_mutex);
                             g_service_observers.emplace(self, std::move(obs));
                         }
                         self->add_observer(ptr);
                     })
        .addFunction("remove_observers", +[](ServiceController *self) {
            std::lock_guard l(g_service_obs_mutex);
            auto range = g_service_observers.equal_range(self);
            for (auto it = range.first; it != range.second; ++it)
                self->remove_observer(it->second.get());
            g_service_observers.erase(self);
        })
        .endClass()
        /**
         * Runnable
         */
        .beginClass<IRunnable>("IRunnable")
        .addFunction("run", &IRunnable::run)
        .endClass()
        .deriveClass<ABlockingService, AService>("ABlockingService")
        .addFunction("set_service_wait_stop", &ABlockingService::set_service_wait_stop)
        .addFunction("is_running", &ABlockingService::is_running)
        .addFunction("is_ready", &ABlockingService::is_ready)
        .addFunction(
            "wait_ready",
            +[](ABlockingService *self, luabridge::LuaRef timeout, lua_State *state) {
                Duration duration = sihd::lua::to_duration(timeout);
                LuaGilRelease release(state);
                return self->wait_ready(duration);
            })
        .endClass()
        .deriveClass<AThreadedService, AService>("AThreadedService")
        .addFunction("set_start_synchronised", &AThreadedService::set_start_synchronised)
        .endClass()
        .endNamespace()
        .endNamespace();
}

void LuaUtilApi::load_all(Vm & vm)
{
    LuaUtilApi::load_base(vm);
    LuaUtilApi::load_tools(vm);
    LuaUtilApi::load_threading(vm);
}

/* ************************************************************************* */
/* LuaCoroutine */
/* ************************************************************************* */

bool LuaUtilApi::LuaCoroutine::create()
{
    if (_parent == nullptr)
        return false;
    // make non owning Vm from the parent state, derive the coroutine into _vm
    Vm parent(_parent);
    return parent.new_thread(_vm);
}

/* ************************************************************************* */
/* LuaScheduler */
/* ************************************************************************* */

LuaUtilApi::LuaScheduler::LuaScheduler(const std::string & name, sihd::util::Node *parent): Scheduler(name, parent) {}

LuaUtilApi::LuaScheduler::~LuaScheduler() = default;

bool LuaUtilApi::LuaScheduler::start()
{
    if (_lua_coroutine.parent() == nullptr)
        return false;
    if (this->is_running())
        return true;

    // create new lua stack for thread
    if (_lua_coroutine.create() == false)
        return false;

    lua_State *state_ptr = _lua_coroutine.state();

    {
        auto l = _waitable_task.guard();
        // point every lua task at the freshly created worker coroutine; tasks that
        // survived a previous stop() (rescheduling ones) live in _task_map and would
        // otherwise keep a dangling _exec_state to the already closed coroutine
        const auto set_state = [state_ptr](sihd::util::Task *task) {
            LuaTask *lua_task = dynamic_cast<LuaTask *>(task);
            if (lua_task != nullptr)
                lua_task->new_lua_state(state_ptr);
        };
        for (const auto & task : _tasks_to_add)
            set_state(task);
        for (const auto & [_, task] : _task_map)
            set_state(task);
    }

    // Enable synchronized start so that the scheduler thread is fully ready before returning
    this->set_start_synchronised(true);

    const bool started = Scheduler::start();
    if (started == false)
        _lua_coroutine.destroy();
    return started;
}

bool LuaUtilApi::LuaScheduler::stop()
{
    const bool success = Scheduler::stop();
    if (success)
    {
        {
            // drop the coroutine pointer from every surviving task before the coroutine
            // is closed, so no task keeps a dangling _exec_state across a restart
            auto l = _waitable_task.guard();
            const auto reset_state = [](sihd::util::Task *task) {
                LuaTask *lua_task = dynamic_cast<LuaTask *>(task);
                if (lua_task != nullptr)
                    lua_task->reset_lua_state();
            };
            for (const auto & task : _tasks_to_add)
                reset_state(task);
            for (const auto & [_, task] : _task_map)
                reset_state(task);
        }
        _lua_coroutine.destroy();
    }
    return success;
}

void LuaUtilApi::LuaScheduler::add_lua_task(LuaTask *task_ptr)
{
    // if thread is running and new task is added, change lua state to thread state
    if (this->is_running() && _lua_coroutine.state() != nullptr)
    {
        task_ptr->new_lua_state(_lua_coroutine.state());
    }
    this->add_task(task_ptr);
}

void LuaUtilApi::LuaScheduler::set_state(lua_State *state)
{
    _lua_coroutine.set_state(state);
}

/* ************************************************************************* */
/* LuaThreadRunner */
/* ************************************************************************* */

LuaUtilApi::LuaThreadRunner::LuaThreadRunner(luabridge::LuaRef lua_ref): _fun(lua_ref) {}

LuaUtilApi::LuaThreadRunner::~LuaThreadRunner()
{
    // the shared function is anchored in the parent universe registry; releasing it is a
    // luaL_unref on the parent state. A run-once task/runnable can be deleted on its
    // scheduler/worker thread, so serialize the unref through the universe GIL. Reset to a
    // nil ref under the GIL; the trailing member dtor then unrefs LUA_REFNIL (no-op).
    LuaGilGuard guard(_fun.state());
    _fun = luabridge::LuaRef(_fun.state());
}

void LuaUtilApi::LuaThreadRunner::reset_lua_state()
{
    _exec_state = nullptr;
}

void LuaUtilApi::LuaThreadRunner::new_lua_state(lua_State *new_state)
{
    _exec_state = new_state != _fun.state() ? new_state : nullptr;
}

luabridge::LuaRef LuaUtilApi::LuaThreadRunner::_exec_fun()
{
    if (_exec_state == nullptr)
        return _fun;
    // caller holds the GIL: move the shared function onto the exec thread stack and ref it there;
    // the returned temp unrefs into the live exec thread, never into a closed coroutine at GC
    _fun.push();
    lua_xmove(_fun.state(), _exec_state, 1);
    return luabridge::LuaRef::fromStack(_exec_state);
}

/* ************************************************************************* */
/* LuaRunnable */
/* ************************************************************************* */

LuaUtilApi::LuaRunnable::LuaRunnable(luabridge::LuaRef lua_ref): LuaThreadRunner(lua_ref) {}

LuaUtilApi::LuaRunnable::~LuaRunnable() = default;

bool LuaUtilApi::LuaRunnable::run()
{
    try
    {
        return this->call_lua_method<bool>();
    }
    catch (const luabridge::LuaException & e)
    {
        SIHD_LOG(error, "{}", e.what());
    }
    return false;
}

/* ************************************************************************* */
/* LuaTask */
/* ************************************************************************* */

LuaUtilApi::LuaTask::LuaTask(luabridge::LuaRef lua_ref, const util::TaskOptions & options):
    Task(nullptr, options),
    LuaThreadRunner(lua_ref)
{
}

LuaUtilApi::LuaTask::~LuaTask() = default;

bool LuaUtilApi::LuaTask::run()
{
    try
    {
        return this->call_lua_method<bool>();
    }
    catch (const luabridge::LuaException & e)
    {
        SIHD_LOG(error, "LuaTask: {}", e.what());
    }
    catch (const std::exception & e)
    {
        SIHD_LOG(error, "LuaTask: {}", e.what());
    }
    return false;
}

/* ************************************************************************* */
/* LuaWorker */
/* ************************************************************************* */

LuaUtilApi::LuaWorker::LuaWorker(luabridge::LuaRef lua_ref): _lua_runnable(lua_ref)
{
    this->set_runnable(&_lua_runnable);
}

LuaUtilApi::LuaWorker::~LuaWorker()
{
    // join before the coroutine anchor is destroyed
    this->stop_worker();
}

void LuaUtilApi::LuaWorker::set_state(lua_State *state)
{
    _lua_coroutine.set_state(state);
}

bool LuaUtilApi::LuaWorker::start_worker(const std::string_view name)
{
    if (this->is_worker_started() == false)
    {
        if (_lua_coroutine.create() == false)
            return false;
        _lua_runnable.new_lua_state(_lua_coroutine.state());
    }
    return Worker::start_worker(name);
}

bool LuaUtilApi::LuaWorker::stop_worker()
{
    // join first, the coroutine is then idle to close
    const bool ret = Worker::stop_worker();
    _lua_runnable.reset_lua_state();
    _lua_coroutine.destroy();
    return ret;
}

/* ************************************************************************* */
/* LuaStepWorker */
/* ************************************************************************* */

LuaUtilApi::LuaStepWorker::LuaStepWorker(luabridge::LuaRef lua_ref): _lua_runnable(lua_ref)
{
    this->set_runnable(&_lua_runnable);
}

LuaUtilApi::LuaStepWorker::~LuaStepWorker()
{
    // join before the coroutine anchor is destroyed
    this->stop_worker();
}

void LuaUtilApi::LuaStepWorker::set_state(lua_State *state)
{
    _lua_coroutine.set_state(state);
}

bool LuaUtilApi::LuaStepWorker::start_worker(const std::string_view name)
{
    if (this->is_worker_started() == false)
    {
        if (_lua_coroutine.create() == false)
            return false;
        _lua_runnable.new_lua_state(_lua_coroutine.state());
    }
    return StepWorker::start_worker(name);
}

bool LuaUtilApi::LuaStepWorker::stop_worker()
{
    // join first, the coroutine is then idle to close
    const bool ret = StepWorker::stop_worker();
    _lua_runnable.reset_lua_state();
    _lua_coroutine.destroy();
    return ret;
}

} // namespace sihd::lua

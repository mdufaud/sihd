#include <memory>
#include <regex>

#include <sihd/lua/util/LuaUtilApi.hpp>

#include <sihd/util/Named.hpp>
#include <sihd/util/Node.hpp>

#include <sihd/util/ABlockingService.hpp>
#include <sihd/util/AService.hpp>
#include <sihd/util/AThreadedService.hpp>
#include <sihd/util/Clocks.hpp>
#include <sihd/util/Splitter.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/endian.hpp>
#include <sihd/util/platform.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/term.hpp>
#include <sihd/util/thread.hpp>
#include <sihd/util/time.hpp>
#include <sihd/util/type.hpp>

#define DECLARE_ARRAY_USERTYPE(ArrType, PrimitiveType)                                                       \
    .deriveClass<ArrType, IArray>(#ArrType)                                                                  \
        .addStaticFunction("new", &LuaUtilApi::_array_lua_new<PrimitiveType>)                                \
        .addConstructor<void (*)()>()                                                                        \
        .addFunction("clone", &ArrType::clone)                                                               \
        .addFunction("push_back", &LuaUtilApi::_array_lua_push_back<PrimitiveType>)                          \
        .addFunction("push_front", &LuaUtilApi::_array_lua_push_front<PrimitiveType>)                        \
        .addFunction("copy_from", &LuaUtilApi::_array_lua_copy_table<PrimitiveType>)                         \
        .addFunction("from", &LuaUtilApi::_array_lua_from<PrimitiveType>)                                    \
        .addFunction("pop", &ArrType::pop)                                                                   \
        .addFunction("front", &ArrType::front)                                                               \
        .addFunction("back", &ArrType::back)                                                                 \
        .addFunction("at", &ArrType::at)                                                                     \
        .addFunction("set", &ArrType::set)                                                                   \
        .addFunction("__len", &ArrType::size)                                                                \
        .endClass()

namespace sihd::lua
{

using namespace sihd::util;
SIHD_LOGGER;

namespace
{
// logger used by lua code
Logger g_lua_logger("sihd::lua");
} // namespace

bool LuaUtilApi::_configurable_recursive_set(Configurable *obj,
                                             const std::string & key,
                                             luabridge::LuaRef ref)
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
        .addFunction("now", static_cast<time_t (LuaScheduler::*)() const>(&Scheduler::now))
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
            +[](LuaScheduler *self, uint32_t val) { self->overrun_at = val; })
        .addProperty(
            "acceptable_task_preplay_ns_time",
            +[](const LuaScheduler *self) { return self->acceptable_task_preplay_ns_time; },
            +[](LuaScheduler *self, uint32_t val) { self->acceptable_task_preplay_ns_time = val; })
        // LuaScheduler
        .addFunction(
            "start",
            std::function<bool(LuaScheduler *, lua_State *)>(+[](LuaScheduler *self, lua_State *state) {
                self->set_state(state);
                return self->start();
            }))
        .addFunction("stop", static_cast<bool (LuaScheduler::*)()>(&Scheduler::stop))
        .addFunction(
            "add_task",
            +[](LuaScheduler *self, luabridge::LuaRef tbl, lua_State *state) {
                if (tbl.isTable() == false)
                    luaL_error(state, "add_task argument must be a table");

                luabridge::LuaRef lua_fun = tbl["task"];
                if (lua_fun.isFunction() == false)
                    luaL_error(state, "add_task table at 'task' must contain a function");

                time_t timestamp_to_run_at = 0;
                luabridge::LuaRef run_at = tbl["run_at"];
                if (run_at.isNumber())
                    timestamp_to_run_at = static_cast<mode_t>(run_at);

                time_t timestamp_to_run_in = 0;
                luabridge::LuaRef run_in = tbl["run_in"];
                if (run_in.isNumber())
                    timestamp_to_run_in = static_cast<mode_t>(run_in);

                time_t reschedule_time = 0;
                luabridge::LuaRef reschedule = tbl["reschedule_time"];
                if (reschedule.isNumber())
                    reschedule_time = static_cast<time_t>(reschedule);

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
        .addFunction("wait", static_cast<void (Waitable::*)()>(&Waitable::wait))
        .addFunction("wait_until", static_cast<bool (Waitable::*)(Timestamp)>(&Waitable::wait_until))
        .addFunction("wait_for", static_cast<bool (Waitable::*)(Timestamp)>(&Waitable::wait_for))
        .addFunction("wait_elapsed", static_cast<Timestamp (Waitable::*)()>(&Waitable::wait_elapsed))
        .addFunction("wait_until_elapsed",
                     static_cast<Timestamp (Waitable::*)(Timestamp)>(&Waitable::wait_until_elapsed))
        .addFunction("wait_for_elapsed",
                     static_cast<Timestamp (Waitable::*)(Timestamp)>(&Waitable::wait_for_elapsed))
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
                             return self->start_sync_worker(name);
                         }))
        .addFunction("stop_worker", static_cast<bool (LuaWorker::*)()>(&Worker::stop_worker))
        .addFunction("is_worker_running",
                     static_cast<bool (LuaWorker::*)() const>(&Worker::is_worker_running))
        .addFunction("is_worker_started",
                     static_cast<bool (LuaWorker::*)() const>(&Worker::is_worker_started))
        .endClass()
        .beginClass<LuaStepWorker>("StepWorker")
        .addConstructor<void (*)(luabridge::LuaRef ref)>()
        .addFunction("set_frequency",
                     static_cast<bool (LuaStepWorker::*)(double)>(&StepWorker::set_frequency))
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
                             return self->start_sync_worker(name);
                         }))
        .addFunction("stop_worker", static_cast<bool (LuaStepWorker::*)()>(&Worker::stop_worker))
        .addFunction("is_worker_running",
                     static_cast<bool (LuaStepWorker::*)() const>(&Worker::is_worker_running))
        .addFunction("is_worker_started",
                     static_cast<bool (LuaStepWorker::*)() const>(&Worker::is_worker_started))
        .addFunction("pause_worker", static_cast<void (LuaStepWorker::*)()>(&StepWorker::pause_worker))
        .addFunction("resume_worker", static_cast<void (LuaStepWorker::*)()>(&StepWorker::resume_worker))
        .addFunction("nano_sleep_time",
                     static_cast<time_t (LuaStepWorker::*)() const>(&StepWorker::nano_sleep_time))
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
        .addConstructor<void (*)(time_t)>()
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
        .addFunction("format",
                     static_cast<std::string (Timestamp::*)(std::string_view) const>(&Timestamp::format))
        .addFunction(
            "local_format",
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
        .endClass()
        /**
         * Namespaces
         */
        .beginNamespace("time")
        // sleep
        .addFunction("nsleep", &time::nsleep)
        .addFunction("usleep", &time::usleep)
        .addFunction("msleep", &time::msleep)
        .addFunction("sleep", &time::sleep)
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
        .addConstructorFrom<SmartNodePtr<Named>, void(const std::string &, Node *)>()
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
        .addConstructorFrom<SmartNodePtr<Node>, void(const std::string &, Node *)>()
        .addFunction("get_child", static_cast<Named *(Node::*)(const std::string &)>(&Node::get_child))
        .addFunction(
            "add_child",
            +[](Node *self, Named *child) { return self->add_child(child); })
        .addFunction(
            "add_child_name",
            +[](Node *self, const std::string & name, Named *child) { return self->add_child(name, child); })
        .addFunction("remove_child", static_cast<bool (Node::*)(const Named *)>(&Node::remove_child))
        .addFunction("remove_child_name",
                     static_cast<bool (Node::*)(const std::string &)>(&Node::remove_child))
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
        .addFunction("service_ctrl", &AService::service_ctrl)
        .endClass()
        .beginClass<ServiceController>("ServiceController")
        .addFunction("state", &ServiceController::state)
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
        .addFunction("wait_ready", &ABlockingService::wait_ready)
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
/* LuaScheduler */
/* ************************************************************************* */

LuaUtilApi::LuaScheduler::LuaScheduler(const std::string & name, sihd::util::Node *parent):
    Scheduler(name, parent),
    _state_ptr(nullptr),
    _vm_thread(nullptr)
{
}

LuaUtilApi::LuaScheduler::~LuaScheduler() = default;

bool LuaUtilApi::LuaScheduler::start()
{
    if (_state_ptr == nullptr)
        return false;
    if (this->is_running())
        return true;

    // create new lua stack for thread

    // make non owning LuaVm from lua_State
    Vm vm(_state_ptr);
    if (vm.new_thread(_vm_thread) == false)
        return false;

    lua_State *state_ptr = _vm_thread.lua_state();

    {
        auto l = _waitable_task.guard();
        for (const auto & task : _tasks_to_add)
        {
            // if lua tasks has been added before thread is created, set new lua state
            LuaTask *lua_task = dynamic_cast<LuaTask *>(task);
            if (lua_task != nullptr)
                lua_task->new_lua_state(state_ptr);
        }
    }

    // Enable synchronized start so that the scheduler thread is fully ready before returning
    this->set_start_synchronised(true);

    const bool started = Scheduler::start();
    if (started == false)
        _vm_thread.close_state();
    return started;
}

bool LuaUtilApi::LuaScheduler::stop()
{
    const bool success = Scheduler::stop();
    if (success)
    {
        _vm_thread.close_state();
    }
    return success;
}

void LuaUtilApi::LuaScheduler::add_lua_task(LuaTask *task_ptr)
{
    // if thread is running and new task is added, change lua state to thread state
    if (this->is_running() && _vm_thread.lua_state() != nullptr)
    {
        task_ptr->new_lua_state(_vm_thread.lua_state());
    }
    this->add_task(task_ptr);
}

void LuaUtilApi::LuaScheduler::set_state(lua_State *state)
{
    _state_ptr = state;
}

/* ************************************************************************* */
/* LuaThreadRunner */
/* ************************************************************************* */

LuaUtilApi::LuaThreadRunner::LuaThreadRunner(luabridge::LuaRef lua_ref): _original_fun(lua_ref), _fun(lua_ref)
{
}

LuaUtilApi::LuaThreadRunner::~LuaThreadRunner() = default;

void LuaUtilApi::LuaThreadRunner::new_lua_state(lua_State *new_state)
{
    if (_fun.state() != new_state)
    {
        // push original function on top of stack
        _original_fun.push();
        // transfer top of stack to top of thread state stack
        lua_xmove(_original_fun.state(), new_state, 1);
        // create function ref from last stack element (without index it pops the value)
        _fun = luabridge::LuaRef::fromStack(new_state, -1);
    }
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

LuaUtilApi::LuaWorker::LuaWorker(luabridge::LuaRef lua_ref): _state_ptr(nullptr), _lua_runnable(lua_ref)
{
    this->set_runnable(&_lua_runnable);
}

LuaUtilApi::LuaWorker::~LuaWorker() = default;

void LuaUtilApi::LuaWorker::set_state(lua_State *state)
{
    _state_ptr = state;
}

bool LuaUtilApi::LuaWorker::start_worker(const std::string_view name)
{
    if (this->is_worker_started() == false)
    {
        // make non owning LuaVm from lua_State
        Vm current_vm(_state_ptr);
        lua_State *state = current_vm.new_luathread();
        if (state == nullptr)
            return false;
        _lua_runnable.new_lua_state(state);
    }
    return Worker::start_worker(name);
}

/* ************************************************************************* */
/* LuaStepWorker */
/* ************************************************************************* */

LuaUtilApi::LuaStepWorker::LuaStepWorker(luabridge::LuaRef lua_ref):
    _state_ptr(nullptr),
    _lua_runnable(lua_ref)
{
    this->set_runnable(&_lua_runnable);
}

LuaUtilApi::LuaStepWorker::~LuaStepWorker() = default;

void LuaUtilApi::LuaStepWorker::set_state(lua_State *state)
{
    _state_ptr = state;
}

bool LuaUtilApi::LuaStepWorker::start_worker(const std::string_view name)
{
    if (this->is_worker_started() == false)
    {
        // make non owning LuaVm from lua_State
        Vm current_vm(_state_ptr);
        lua_State *state = current_vm.new_luathread();
        if (state == nullptr)
            return false;
        _lua_runnable.new_lua_state(state);
    }
    return StepWorker::start_worker(name);
}

} // namespace sihd::lua

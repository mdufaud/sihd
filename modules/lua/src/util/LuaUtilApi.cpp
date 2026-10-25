#include <map>
#include <memory>
#include <regex>
#include <vector>

#include <sihd/lua/util/LuaUtilApi.hpp>
#include <sihd/lua/checked.hpp>
#include <sihd/sys/console.hpp>
#include <sihd/util/ABlockingService.hpp>
#include <sihd/util/AService.hpp>
#include <sihd/util/AThreadedService.hpp>
#include <sihd/util/Clocks.hpp>
#include <sihd/util/LoggerManager.hpp>
#include <sihd/util/Named.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/Random.hpp>
#include <sihd/util/Splitter.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/build.hpp>
#include <sihd/util/endian.hpp>
#include <sihd/util/num.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/thread.hpp>
#include <sihd/util/time.hpp>
#include <sihd/util/type.hpp>

#define DECLARE_ARRAY_USERTYPE(ArrType, PrimitiveType)                                                                 \
    .deriveClass<ArrType, IArray>(#ArrType)                                                                            \
        .addStaticFunction("new", checked<&array_lua_new<PrimitiveType>>)                                                       \
        .addConstructor<void (*)()>()                                                                                  \
        .addFunction("clone", &ArrType::clone)                                                                         \
        .addFunction("push_back", &array_lua_push_back<PrimitiveType>)                                                 \
        .addFunction("push_front", &array_lua_push_front<PrimitiveType>)                                               \
        .addFunction("copy_from", &array_lua_copy_table<PrimitiveType>)                                                \
        .addFunction("from", &array_lua_from<PrimitiveType>)                                                           \
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
Logger g_lua_logger("sihd::lua");

// Bridges an Observable<ServiceController> notification to a Lua callback; the notify may fire on
// a service thread, so run through a LuaThreadRunner (own coroutine + universe GIL)
class LuaServiceObserver: public IHandler<ServiceController *>
{
    public:
        LuaServiceObserver(lua_State *state, luabridge::LuaRef fun): _runner(fun)
        {
            Vm vm(state);
            lua_State *thread = vm.new_luathread();
            if (thread != nullptr)
                _runner.new_lua_state(thread);
        }

        void handle(ServiceController *ctrl) override { _runner.call_lua_method_noret<ServiceController *>(ctrl); }

    private:
        LuaUtilApi::LuaThreadRunner _runner;
};

template <typename T>
Result<Array<T>> array_lua_new(luabridge::LuaRef ref)
{
    Array<T> array;
    if constexpr (std::is_same_v<T, char>)
    {
        if (ref.isString())
        {
            array.from(std::string(ref));
            return array;
        }
    }
    if (ref.isTable() == false)
        return std::unexpected(Error(ErrorCode::invalid_argument, "Array new argument must be a table"));
    array.reserve(ref.length());
    for (const auto & pair : luabridge::pairs(ref))
        array.push_back(T(pair.second));
    return array;
}

// every C++ local dies in here: the factory raises from a frame that only holds the node pointer
template <typename T>
int new_node(lua_State *state, T *& node)
{
    auto name = luabridge::Stack<std::string>::get(state, 2);
    if (!name)
        return push_error(state, Error(ErrorCode::invalid_argument, "argument 1: {}", name.message()));
    Node *parent = nullptr;
    if (!lua_isnoneornil(state, 3))
    {
        auto parent_arg = luabridge::Stack<Node *>::get(state, 3);
        if (!parent_arg)
            return push_error(state, Error(ErrorCode::invalid_argument, "argument 2: {}", parent_arg.message()));
        parent = *parent_arg;
    }
    auto created = std::make_unique<T>(*name);
    if (parent != nullptr)
    {
        auto added = parent->add_child(created.get(), false);
        if (!added)
            return push_error(state, added.error());
    }
    node = created.release();
    return 0;
}

template <typename T>
bool array_lua_push_back(Array<T> *self, luabridge::LuaRef ref)
{
    bool ret = true;
    if constexpr (std::is_same_v<T, char>)
    {
        if (ref.isString())
        {
            self->push_back(std::string(ref));
            return true;
        }
    }
    if (ref.isTable())
    {
        self->reserve(self->size() + ref.length());
        for (const auto & pair : luabridge::pairs(ref))
            ret = ret && self->push_back(T(pair.second));
    }
    else
        ret = self->push_back(T(ref));
    return ret;
}

template <typename T>
bool array_lua_push_front(Array<T> *self, luabridge::LuaRef ref)
{
    bool ret = true;
    if constexpr (std::is_same_v<T, char>)
    {
        if (ref.isString())
        {
            self->push_front(std::string(ref));
            return true;
        }
    }
    if (ref.isTable())
    {
        self->reserve(self->size() + ref.length());
        for (const auto & pair : luabridge::pairs(ref))
            ret = ret && self->push_front(T(pair.second));
    }
    else
        ret = self->push_front(T(ref));
    return ret;
}

template <typename T>
bool array_lua_copy_table(Array<T> *self, luabridge::LuaRef ref, luabridge::LuaRef from_ref)
{
    size_t from_idx = 0;
    if (from_ref.isNil() == false)
        from_idx = static_cast<size_t>(from_ref);
    if constexpr (std::is_same_v<T, char>)
    {
        if (ref.isString())
        {
            self->copy_from(std::string(ref), from_idx);
            return true;
        }
    }
    if (ref.isTable() == false)
        return false;
    if ((size_t)ref.length() > (from_idx + self->size()))
        return false;
    for (const auto & pair : luabridge::pairs(ref))
    {
        self->set(from_idx, T(pair.second));
        ++from_idx;
    }
    return true;
}

template <typename T>
bool array_lua_from(Array<T> *self, luabridge::LuaRef ref)
{
    bool ret = true;
    if constexpr (std::is_same_v<T, char>)
    {
        if (ref.isString())
        {
            self->from(std::string(ref));
            return true;
        }
    }
    self->delete_buffer();
    if (ref.isTable())
    {
        self->reserve(self->size() + ref.length());
        for (const auto & pair : luabridge::pairs(ref))
            ret = ret && self->push_back(T(pair.second));
    }
    else
        ret = self->push_back(T(ref));
    return ret;
}

void bind_task_state(Task *task, lua_State *state)
{
    LuaUtilApi::LuaTask *lua_task = dynamic_cast<LuaUtilApi::LuaTask *>(task);
    if (lua_task != nullptr)
        lua_task->new_lua_state(state);
}

void unbind_task_state(Task *task)
{
    LuaUtilApi::LuaTask *lua_task = dynamic_cast<LuaUtilApi::LuaTask *>(task);
    if (lua_task != nullptr)
        lua_task->reset_lua_state();
}

std::mutex g_service_obs_mutex;
std::multimap<ServiceController *, std::unique_ptr<LuaServiceObserver>> g_service_observers;
} // namespace

Duration to_duration(const luabridge::LuaRef & ref)
{
    if (ref.isNumber())
        return Duration(ref.cast<int64_t>().value());
    return ref.cast<Duration>().value();
}

Timestamp to_timestamp(const luabridge::LuaRef & ref)
{
    if (ref.isNumber())
        return Timestamp(ref.cast<int64_t>().value());
    return ref.cast<Timestamp>().value();
}

lua_State *LuaUtilApi::LuaThreadRunner::_guard_state()
{
    return _exec_state != nullptr ? _exec_state : _fun.state();
}

void LuaUtilApi::LuaCoroutine::set_state(lua_State *parent)
{
    _parent = parent;
}

lua_State *LuaUtilApi::LuaCoroutine::parent() const
{
    return _parent;
}

lua_State *LuaUtilApi::LuaCoroutine::state() const
{
    return _vm.lua_state();
}

void LuaUtilApi::LuaCoroutine::destroy()
{
    _vm.close_state();
}

bool LuaUtilApi::_configurable_recursive_set(Configurable *obj, const std::string & key, luabridge::LuaRef ref)
{
    switch (ref.type())
    {
        case LUA_TBOOLEAN:
        {
            return obj->set_conf<bool>(key, static_cast<bool>(ref)).has_value();
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
            auto floated = obj->set_conf_float(key, static_cast<double>(ref));
            if (floated.has_value() || floated.error().code != ErrorCode::not_supported)
                return floated.has_value();
            return obj->set_conf_int(key, static_cast<int64_t>(ref)).has_value();
        }
        case LUA_TSTRING:
        {
            return obj->set_conf_str(key, std::string(ref)).has_value();
        }
        default:
        {
            g_lua_logger.error("Configuration key '{}' type error", key);
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
         * Task policies
         */
        .beginNamespace("LatenessPolicy")
        .addVariable("replay_missed", LatenessPolicy::replay_missed)
        .addVariable("push_back", LatenessPolicy::push_back)
        .addVariable("skip_missed", LatenessPolicy::skip_missed)
        .endNamespace()
        .beginNamespace("IdlePolicy")
        .addVariable("sleep", IdlePolicy::sleep)
        .addVariable("sleep_then_spin", IdlePolicy::sleep_then_spin)
        .endNamespace()
        /**
         * Scheduler
         */
        .deriveClass<LuaScheduler, Named>("Scheduler")
        .addConstructorFrom<SmartNodePtr<LuaScheduler>, void(const std::string &, Node *)>()
        .addFunction("set_conf", checked<&LuaUtilApi::configurable_set_conf<LuaScheduler>>)
        .addFunction("is_running", static_cast<bool (LuaScheduler::*)() const>(&Scheduler::is_running))
        .addFunction("pause", static_cast<void (LuaScheduler::*)()>(&Scheduler::pause))
        .addFunction("resume", static_cast<void (LuaScheduler::*)()>(&Scheduler::resume))
        .addFunction("now", static_cast<Timestamp (LuaScheduler::*)() const>(&Scheduler::now))
        .addFunction("clock", static_cast<IClock *(LuaScheduler::*)() const>(&Scheduler::clock))
        .addFunction("set_clock", static_cast<void (LuaScheduler::*)(IClock *)>(&Scheduler::set_clock))
        .addFunction("set_no_delay", static_cast<bool (LuaScheduler::*)(bool)>(&Scheduler::set_no_delay))
        .addFunction("set_idle_policy", static_cast<bool (LuaScheduler::*)(IdlePolicy)>(&Scheduler::set_idle_policy))
        .addFunction("idle_policy", static_cast<IdlePolicy (LuaScheduler::*)() const>(&Scheduler::idle_policy))
        .addFunction("set_spin_window", static_cast<bool (LuaScheduler::*)(Duration)>(&Scheduler::set_spin_window))
        .addFunction("spin_window", static_cast<Duration (LuaScheduler::*)() const>(&Scheduler::spin_window))
        .addFunction("set_skip_missed_on_start",
                     static_cast<bool (LuaScheduler::*)(bool)>(&Scheduler::set_skip_missed_on_start))
        .addFunction("skip_missed_on_start",
                     static_cast<bool (LuaScheduler::*)() const>(&Scheduler::skip_missed_on_start))
        .addFunction("clear_tasks", static_cast<void (LuaScheduler::*)()>(&Scheduler::clear_tasks))
        .addProperty(
            "overruns",
            +[](const LuaScheduler *self) -> size_t { return self->overruns.load(); })
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
            checked<+[](LuaScheduler *self, luabridge::LuaRef tbl) -> Result<void> {
                if (tbl.isTable() == false)
                    return std::unexpected(Error(ErrorCode::invalid_argument, "add_task argument must be a table"));

                luabridge::LuaRef lua_fun = tbl["task"];
                if (lua_fun.isFunction() == false)
                    return std::unexpected(
                        Error(ErrorCode::invalid_argument, "add_task table at 'task' must contain a function"));

                Timestamp timestamp_to_run_at(0);
                luabridge::LuaRef run_at = tbl["run_at"];
                if (run_at.isNil() == false)
                    timestamp_to_run_at = sihd::lua::to_timestamp(run_at);

                Duration timestamp_to_run_in(0);
                luabridge::LuaRef run_in = tbl["run_in"];
                if (run_in.isNil() == false)
                    timestamp_to_run_in = sihd::lua::to_duration(run_in);

                Duration reschedule_time(0);
                luabridge::LuaRef reschedule = tbl["reschedule_time"];
                if (reschedule.isNil() == false)
                    reschedule_time = sihd::lua::to_duration(reschedule);

                LatenessPolicy late_policy = LatenessPolicy::replay_missed;
                luabridge::LuaRef late_policy_ref = tbl["late_policy"];
                if (late_policy_ref.isNil() == false)
                {
                    luabridge::TypeResult<LatenessPolicy> policy_res = late_policy_ref.cast<LatenessPolicy>();
                    if (static_cast<bool>(policy_res) == false)
                        return std::unexpected(Error(ErrorCode::invalid_argument,
                                                     "add_task table at 'late_policy' is not a valid LatenessPolicy"));
                    late_policy = policy_res.value();
                }

                LuaTask *task_ptr = new LuaTask(lua_fun,
                                                util::TaskOptions {.run_at = timestamp_to_run_at,
                                                                   .run_in = timestamp_to_run_in,
                                                                   .reschedule_time = reschedule_time,
                                                                   .late_policy = late_policy});
                if (task_ptr != nullptr)
                {
                    self->add_lua_task(task_ptr);
                }
                return {};
            }>)
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
         * Random: lua integers are signed, seeds and bounds cross as int64 bit patterns
         */
        .beginClass<Random>("Random")
        .addFactory(
            +[](lua_State *L) -> Random * {
                return new Random(static_cast<uint64_t>(*luabridge::Stack<int64_t>::get(L, 2)));
            },
            +[](Random *random) { delete random; })
        .addFunction(
            "rand",
            +[](Random *self, int64_t from, int64_t to) -> int64_t {
                if (from > to)
                    std::swap(from, to);
                const uint64_t span = static_cast<uint64_t>(to) - static_cast<uint64_t>(from);
                return static_cast<int64_t>(static_cast<uint64_t>(from) + self->rand(0, span));
            })
        .addFunction(
            "drand",
            +[](Random *self, double from, double to) { return self->drand(from, to); })
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
        .addFunction("format", static_cast<std::string (Timestamp::*)(const std::string &) const>(&Timestamp::format))
        .addFunction("local_format",
                     static_cast<std::string (Timestamp::*)(const std::string &) const>(&Timestamp::local_format))
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
        .addFunction("to_us", &time::to_micro)
        .addFunction("to_ms", &time::to_milli)
        .addFunction("to_sec", &time::to_sec)
        .addFunction("to_min", &time::to_min)
        .addFunction("to_hours", &time::to_hours)
        .addFunction("to_days", &time::to_days)
        .addFunction("to_double", &time::to_double)
        .addFunction("to_hz", &time::to_freq)
        .addFunction("us", &time::micro)
        .addFunction("ms", &time::milli)
        .addFunction("sec", &time::sec)
        .addFunction("min", &time::min)
        .addFunction("hours", &time::hours)
        .addFunction("days", &time::days)
        .addFunction("from_double", &time::from_double)
        .addFunction("hz", &time::freq)
        .endNamespace()
        .beginNamespace("thread")
        .addFunction("id", &thread::id)
        .addFunction("id_str", thread::id_str)
        .addFunction(
            "set_name",
            checked<+[](const std::string & name) { return thread::set_name(name); }>)
        .addFunction("name", &thread::name)
        .endNamespace()
        .beginNamespace("str")
        .addFunction("timeoffset_str", &str::timeoffset_str)
        .addFunction("localtimeoffset_str", &str::localtimeoffset_str)
        .addFunction("trim", &str::trim)
        .addFunction(
            "replace",
            +[](std::string_view s, std::string_view from, std::string_view to) { return str::replace(s, from, to); })
        .addFunction("starts_with", &str::starts_with)
        .addFunction("ends_with", &str::ends_with)
        .addFunction(
            "split",
            +[](const std::string & str, const std::string & delimiter) {
                Splitter splitter(delimiter);
                return splitter.split(str);
            })
        .endNamespace()
        .beginNamespace("num")
        .addFunction(
            "seed",
            +[] { return static_cast<int64_t>(num::seed()); })
        .endNamespace()
        .beginNamespace("term")
        .addFunction("is_interactive", &sihd::sys::console::is_interactive)
        .endNamespace()
        .beginNamespace("endian")
        .addFunction(
            "is_big",
            +[] { return std::endian::native == std::endian::big; })
        .addFunction(
            "is_little",
            +[] { return std::endian::native == std::endian::little; })
        .endNamespace()
        .endNamespace()
        .endNamespace();
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
        .addFunction(
            "console",
            +[]() { LoggerManager::console(); })
        .addFunction(
            "stream",
            +[]() { LoggerManager::stream(); })
        .addFunction(
            "clear",
            +[]() { LoggerManager::clear_loggers(); })
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
                Named *node = nullptr;
                if (new_node(L, node) < 0)
                    lua_error(L);
                return node;
            },
            +[](Named *node) { delete node; })
        .addProperty(
            "c_ptr",
            +[](const Named *self) -> int64_t { return (int64_t)self; })
        .addFunction(
            "parent",
            +[](Named *self) -> Node * { return self->parent(); })
        .addFunction("name", &Named::name)
        .addFunction("full_name", &Named::full_name)
        .addFunction("class_name", &Named::class_name)
        .addFunction("description", &Named::description)
        .addFunction("full_name", &Named::full_name)
        .addFunction(
            "root",
            +[](Named *self) -> Node * { return self->root(); })
        .addFunction(
            "find",
            +[](Named *self, const std::string & path) -> Named * { return self->find(path); })
        .addFunction(
            "__eq",
            +[](const Named *self, const Named *other) -> bool { return self == other; })
        .addIndexMetaMethod([](Named & self, const luabridge::LuaRef & key, lua_State *L) {
            Named *res = self.find(key.tostring());
            if (res == nullptr)
                return luabridge::LuaRef(L, luabridge::LuaNil());
            return luabridge::LuaRef(L, res);
        })
        .endClass()
        .deriveClass<Node, Named>("Node")
        .addFactory(
            +[](lua_State *L) -> Node * {
                Node *node = nullptr;
                if (new_node(L, node) < 0)
                    lua_error(L);
                return node;
            },
            +[](Node *node) { delete node; })
        .addFunction(
            "get_child",
            +[](Node *self, const std::string & name) -> Named * { return self->get_child(name); })
        .addFunction(
            "add_child",
            checked<+[](Node *self, Named *child) { return self->add_child(child, false); }>)
        .addFunction(
            "add_child_name",
            checked<+[](Node *self, const std::string & name, Named *child) { return self->add_child(name, child, false); }>)
        .addFunction("remove_child", static_cast<bool (Node::*)(const Named *)>(&Node::remove_child))
        .addFunction("remove_child_name", static_cast<bool (Node::*)(const std::string &)>(&Node::remove_child))
        .addFunction("is_link", &Node::is_link)
        .addFunction("add_link", &Node::add_link)
        .addFunction("remove_link", &Node::remove_link)
        .addFunction("resolve_link", &Node::resolve_link)
        .addFunction(
            "resolve_links",
            checked<+[](Node *self) { return self->resolve_links(); }>)
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
        .addFunction(
            "service_ctrl",
            +[](AService *self) -> ServiceController * {
                return dynamic_cast<ServiceController *>(self->service_ctrl());
            })
        .endClass()
        .beginClass<ServiceController>("ServiceController")
        .addFunction("state", &ServiceController::state)
        .addFunction(
            "state_str",
            +[](ServiceController *self) -> std::string { return ServiceController::state_str(self->state()); })
        .addFunction(
            "add_observer",
            checked<+[](ServiceController *self, luabridge::LuaRef fun, lua_State *state) -> Result<void> {
                if (fun.isFunction() == false)
                    return std::unexpected(Error(ErrorCode::invalid_argument, "add_observer: expected a function"));
                auto obs = std::make_unique<LuaServiceObserver>(state, fun);
                LuaServiceObserver *ptr = obs.get();
                {
                    std::lock_guard lock(g_service_obs_mutex);
                    g_service_observers.emplace(self, std::move(obs));
                }
                LuaGilRelease release(state);
                self->add_observer(ptr);
                return {};
            }>)
        .addFunction(
            "remove_observers",
            +[](ServiceController *self, lua_State *state) {
                std::vector<std::unique_ptr<LuaServiceObserver>> removed;
                {
                    std::lock_guard lock(g_service_obs_mutex);
                    auto range = g_service_observers.equal_range(self);
                    for (auto it = range.first; it != range.second; ++it)
                        removed.push_back(std::move(it->second));
                    g_service_observers.erase(self);
                }
                LuaGilRelease release(state);
                for (const auto & obs : removed)
                    self->remove_observer(obs.get());
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

bool LuaUtilApi::LuaCoroutine::create()
{
    if (_parent == nullptr)
        return false;
    Vm parent(_parent);
    return parent.new_thread(_vm);
}

LuaUtilApi::LuaScheduler::LuaScheduler(const std::string & name, Node *parent): Scheduler(name, parent) {}

LuaUtilApi::LuaScheduler::~LuaScheduler()
{
    // join before the coroutine anchor is destroyed
    if (this->is_running())
        this->stop();
}

bool LuaUtilApi::LuaScheduler::start()
{
    if (_lua_coroutine.parent() == nullptr)
        return false;
    if (this->is_running())
        return true;

    if (_lua_coroutine.create() == false)
        return false;

    lua_State *state_ptr = _lua_coroutine.state();

    {
        auto guard = _waitable_task.guard();
        // Point every lua task at the freshly created worker coroutine: tasks that survived a previous
        // stop() (rescheduling ones) live in _task_map and would keep a dangling _exec_state
        for (const auto & task : _tasks_to_add)
            bind_task_state(task, state_ptr);
        for (const auto & [_, task] : _task_map)
            bind_task_state(task, state_ptr);
    }

    this->set_start_synchronised(true);

    const bool started = Scheduler::start();
    if (started == false)
        _lua_coroutine.destroy();
    return started;
}

bool LuaUtilApi::LuaScheduler::stop()
{
    bool success = false;
    {
        // GIL released: a GC finalizer holds it while a running task waits on it
        LuaGilRelease release(_lua_coroutine.state());
        success = Scheduler::stop();
    }
    if (success)
    {
        {
            // drop the coroutine pointer from every surviving task before the coroutine
            // is closed, so no task keeps a dangling _exec_state across a restart
            auto guard = _waitable_task.guard();
            for (const auto & task : _tasks_to_add)
                unbind_task_state(task);
            for (const auto & [_, task] : _task_map)
                unbind_task_state(task);
        }
        _lua_coroutine.destroy();
    }
    return success;
}

void LuaUtilApi::LuaScheduler::add_lua_task(LuaTask *task_ptr)
{
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

LuaUtilApi::LuaThreadRunner::LuaThreadRunner(luabridge::LuaRef lua_ref): _fun(lua_ref) {}

LuaUtilApi::LuaThreadRunner::~LuaThreadRunner()
{
    // The shared function anchors in the parent universe registry (luaL_unref on the parent state);
    // run-once tasks can die on their worker thread, so serialize the unref through the universe GIL
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
    bool ret = false;
    {
        // GIL released: a GC finalizer holds it while the worker waits on it
        LuaGilRelease release(_lua_coroutine.state());
        ret = Worker::stop_worker();
    }
    _lua_runnable.reset_lua_state();
    _lua_coroutine.destroy();
    return ret;
}

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
    bool ret = false;
    {
        // GIL released: a GC finalizer holds it while the worker waits on it
        LuaGilRelease release(_lua_coroutine.state());
        ret = StepWorker::stop_worker();
    }
    _lua_runnable.reset_lua_state();
    _lua_coroutine.destroy();
    return ret;
}

} // namespace sihd::lua

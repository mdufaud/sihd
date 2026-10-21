# util

`util` is the base layer used by every other sihd module. It provides logging, service lifecycle and threading primitives, observers and callbacks, a named tree (`Named`/`Node`) with typed values and messages, time and duration types, typed arrays and containers, string and regex helpers, a `Result`/`Error` error model, and a CLI application framework. Most of the library depends on it, so its headers are the first place to look for a utility before writing one.

| | |
|---|---|
| Depends on | `json` |
| External libraries | fmt, CLI11, pcre2 (plus pthread on Linux and Windows, log on Android) |
| Platforms | all |
| Build | `make m=util` |
| Header | `#include <sihd/util/Logger.hpp>` (one header per class or namespace) |

## Capabilities

### Logging

- `Logger`: named logger created with `SIHD_NEW_LOGGER("name")` and used through `SIHD_LOGGER`; `log`, `emergency` to `debug`, with `fmt`-style formatting.
- `SIHD_LOG(level, ...)`, `SIHD_LOG_FORMAT`, `SIHD_LOG_LVL`: log macros on the module logger. `SIHD_TRACE*` macros are compiled out when `SIHD_TRACE_OFF` is set. `SIHD_COUT`, `SIHD_CERR`, `SIHD_COUTV`: formatted output helpers.
- `LogInfo`, `LogLevel`: per-message metadata (source, level, timestamp, thread) and the level enum with `level_str` and `level_from_str`.
- `LoggerManager`: process-wide registry of sinks and filters. Static `stream`, `console`, `thrower`, `async` install sinks; `add`, `rm`, `filter`, `rm_filter`, `set_level`, `should_log`, `clear_loggers`, `clear_filters`.
- `TmpLoggerAdder`, `TmpLoggerFilterAdder`: RAII helpers that register a logger or filter for the duration of a scope.
- `ALogger`: base class for sinks; subclasses implement `log(const LogInfo &, std::string_view)`.
- `LoggerConsole`, `LoggerStream` (formatted by `set_pattern`), `LoggerThrow` (throws on each logged message), `LoggerAsync` (queues messages to a target sink; `dropped()` counts refused messages).
- `ALogFilterer`, `ILoggerFilter`, `LoggerFilter` (`Options`: message, source and thread regexes, thread id and level comparisons): message filtering.
- `LogFormatter`: pattern-driven message formatting (`set_pattern`, `format`).
- `log_unexpected`, `log_unexpected_error`: log the error of a failed `Result`.

### Services

- `AService`: lifecycle `setup`, `init`, `start`, `stop`, `reset`, each with a `do_*` hook; `service_state()` is an `Observable` of state changes.
- `ServiceController`: state machine behind a service (`state`, `last_event`, `state_str`); `ThreadedServiceController` and `BlockingServiceController` are the threaded and blocking variants.
- `AThreadedService`: runs `on_start` and `on_stop` on a service thread (`set_service_nb_thread`, `set_start_synchronised`, `thread_name`).
- `ABlockingService`: blocking service with `wait_ready`, `is_ready`, `set_service_wait_stop`.
- `AWorkerService`: threaded service whose `run` loop calls `on_work_start` and `on_work_stop`.
- `AStepWorkerService`: threaded service stepped at a fixed rate (`set_step_frequency`) with `on_work_setup`, `on_work_start`, `on_work_stop`, `on_work_teardown` hooks.
- `IServiceController`-based state transitions are driven by `StateMachine` and `IStateMachine` (`transition`, `state`, `state_name`, `set_transitions_map`).

### Threads and scheduling

- `Worker`: runs a method or `IRunnable` on a thread (`start_worker`, `start_sync_worker`, `stop_worker`, `set_runnable`, `set_method`).
- `StepWorker`: `Worker` that calls its runnable at a fixed frequency (`set_frequency`, `frequency`, `pause_worker`, `resume_worker`, `set_callback_setup`, `set_callback_teardown`).
- `ThreadPool`: fixed-size pool; `add_job` returns a `std::future`, `complete_all_jobs`, `wait_all_jobs`, `remaining_jobs`, `stats`.
- `IRunnable`, `Runnable`: interface and wrapper for a `bool run()` unit of work (from a function, a method, or another `IRunnable`).
- `Task`, `TaskOptions`, `LatenessPolicy`: a runnable scheduled at `run_at` or `run_in`, rescheduled from the previous run by `reschedule_time`; `LatenessPolicy` selects `replay_missed`, `push_back` or `skip_missed` for late runs.
- `Scheduler`: service that plays `Task`s in time order (`add_task`, `remove_task`, `clear_tasks`, `pause`, `resume`, `set_clock`, `set_idle_policy` with `IdlePolicy`, `set_spin_window`, `set_skip_missed_on_start`). Configurable.
- `Timer`: one-shot callback run after a `Duration` unless `cancel()` is called first.
- `TimedHandler`: wraps an `IHandler` and records how long each call takes (`stat`, `last`, `set_active`).
- `RateLimiter`: allows at most N events per window (`allow`, `retry_after`, `full`, `reset`).
- `TokenBucket`: token-bucket limiter (`try_consume`, `tokens`, `time_for`, `reset`).
- `Waitable`, `WaitableRecursive`: condition-variable wrapper with `notify`, `notify_all`, `wait`, `wait_for`, `wait_until` taking a predicate, plus `*_elapsed` variants.
- `Synchronizer`: barrier for a fixed number of parties (`init_sync`, `sync`, `sync(timeout)`, `reset`).
- `SafeQueue`: thread-safe queue with optional bounded size (`push`, `push_lazy`, `try_pop`, `pop` returning `Result`, `pop_wait`, `terminate`, `wait_for_space`, `peek_front`, `peek_back`).

### Observers and callbacks

- `Observable<T>`: notifies registered `IHandler<T *>` observers (`add_observer`, `remove_observer`, `is_observer`, `observers_count`, `set_watcher`).
- `ObservableDelegate<T>`: `Observable` that notifies on behalf of another object (`notify_observers_delegate`).
- `IObserverWatcher`, `ObserverWatcher`: hooks around each notification and each observer call.
- `ObserverWaiter<T>`: blocks until observed notifications arrive (`observe`, `wait`, `wait_for`, `wait_nb`, `wait_for_nb`, `prev_wait`, `prev_wait_for`, `clear`). `prev_wait` variants are for a write and a wait on the same thread.
- `IHandler<Args...>`, `Handler<Args...>`: handler interface and std-function-backed implementation (`set_method`, `set_handler`).
- `CallbackManager`: named callbacks with typed signatures (`set`, `call`, `remove`, `check_call_type`); throws `std::out_of_range` for an unknown name and `std::invalid_argument` on a signature mismatch.
- `MoveOnlyFunction<R(Args...)>`: move-only `std::function`-like wrapper.
- `Collector<T>`: service that polls an `IProvider<T>` and notifies observers with each item (`collect`, `set_provider`, `set_timeout`, `data`).

### Command line

- `CliApp`: application framework with `Options` (name, description, version, logging setup), `run(argc, argv)`, `loop`, `wait_for_termination`, `stop`, `reload`, `set_conf_loader`, and overridable `on_boot`, `on_configure`, `on_run`, `on_terminate`, `on_halt`, `on_fail` hooks.
- `CliInterpreter`: command tree evaluator (`evaluate` on argument lists or raw lines), with generated help and `bash_completion`.
- `Command`: a node of the command tree (`add_command`, `path`, `command_names`, `bind`, `bind_positional`, `on_run`) that binds C++ variables to options.
- `TokenPattern`: compiles a `{token}` template (`compile`) and renders it from a provider (`render`).
- `ansi`: escape-sequence constants in `ansi::attr` (bold, underline, colors) and cursor helpers (`move_cursor_up`, `set_pos`, `next_line`, `color_supported`, and styling functions).

### Time

- `Timestamp`: absolute time since the epoch. `now`, `from_str`, `str`, `local_str`, `format`, `local_format`, `calendar`, `clocktime`, `floor_day`, `modulo_min`, `in_interval`, `ts`, `tv`, `tm`.
- `Duration`: signed time span with `str`; `TimeBase<T>`: shared arithmetic and conversions (`nanoseconds` to `days`, `floor`, `duration`).
- `Clocktime`, `Calendar`: broken-down time structures.
- `IClock`, `SteadyClock`, `SystemClock`, `Clock`: clock abstraction. `Scheduler`, `RateLimiter` and `TokenBucket` accept an injected clock through `set_clock`.
- `Stopwatch`: elapsed time since `reset` (`time`, `reset`).
- `time.hpp`: free functions for unit conversion (`sec`, `milli`, `micro`, `to_sec`, `to_milli`, `to_duration`), `sleep`, `msleep`, `usleep`, `nsleep`, `to_tm`, `to_ts`, timezone helpers (`get_timezone`, `get_timezone_name`), and the aliases `days`, `weeks`, `months`, `years`.

### Containers

- `Array<T>`: resizable typed array with byte-level access (`copy_from_bytes`, `copy_to_bytes`, `from_bytes`, `from_str`, `byte_resize`, `assign_bytes`, `str`, `hexdump`). `ArrayUnique`, `ArrayShared`, `ArrayWeak` are smart-pointer aliases.
- `ArrayView<T>`: non-owning view (`subview`, `remove_prefix`, `remove_suffix`, `copy_to`, `is_equal`, `str`).
- `IArray`, `IArrayView`: type-erased interfaces (`data_type`, `size`, `byte_size`, `resize`, `from_bytes`, `copy_to_bytes`).
- `ArrayIterator`, `ReverseArrayIterator`: iterators returned by `Array` and `ArrayView`.
- `RingBuffer<T>`: fixed-capacity ring (`write`, `read`, `skip`, `write_span`, `produce`, `read_span`).
- `Slice`: half-open index range with `resolve` and `from_size`, used by the byte-copy methods.
- `Cache<Key, Value>`: keyed cache with per-entry getters, TTL and staleness (`set`, `get`, `get_optional`, `refresh_stale`, `refresh_all`, `invalidate`, `entry_stats`, `stats`, `stale_entries`).
- `Splitter`, `SplitterOptions`: string tokenizer with delimiter, escape sequences and enclosures (`split`, `split_view`, `count_tokens`, `next_token`).
- `container.hpp`: generic helpers on STL-like containers (`find`, `find_if`, `contains`, `sort`, `erase`, `erase_if`, `emplace_back_unique`, `get_or`, `recursive_get`, `ordered_keys`, `sum`, `average`, `transform_if`, `insert_at_end`).
- `array_utils.hpp`: `read`, `read_into`, `write` on a typed `IArray` by index, `create_from_type`, `distribute_array`.
- `SmartNodePtr<T>`: `unique_ptr` alias for `Named`-derived nodes, with a custom deleter (`SmartNodeDeleter`).

### Named tree and values

- `Named`: object with a name and an optional parent. `name`, `full_name`, `parent`, `root`, `find` (dotted paths such as `parent.child` and `..`), `set_parent`, ownership flags.
- `Node`: `Named` with children. `add_child` (with or without ownership), `get_child`, `remove_child`, `has_ownership`, `execute_children`, `add_link` and `resolve_link` for named links, `tree_str`.
- `NamedFactory`: `SIHD_REGISTER_FACTORY(Class)` registers a class for lookup by name; `create_from_factory_table` resolves a class name.
- `Value`: type-tagged scalar (`Value(T)`, `compare`, `compare_double_epsilon`, `to_double`, `str`, `from_*_string` parsers).
- `Message`, `DynMessage`: `Node` holding typed `MessageField`s with a byte layout (`add_field`, `get`, `set`, `from_bytes`, `finish`); `DynMessage` adds per-field rules and hidden fields (`add_rule`, `hide_field`, `reset`).
- `MessageField`, `IMessageField`: one typed field, scalar or array (`build_array`, `read_value`, `write_value`, `field_read_from`, `field_write_to`).
- `Provider<Container>`, `VectorProvider`, `ListProvider`, `SetProvider`, `DequeProvider`, `ArrayProvider`, `FunctionProvider`, `IProvider<T>`: pull-style sources of values (`provide`, `providing`).
- `IReader`, `IReaderTimestamp`, `IWriter`, `IWriterTimestamp`: byte-stream interfaces, with variants carrying a `Timestamp`.
- `ICloneable<T>`: interface for `clone()`.

### Configuration

- `Configurable`: registers named setters (`add_conf` with a member function, a free function or a `std::function`) and applies them from a JSON document or one value (`set_conf`, `set_conf_int`, `set_conf_float`, `set_conf_str`). Returns `Result<void>`.
- `StrConfiguration`: parses a `key=value` string (`parse_configuration`, `has`, `find`, `get`, `str`).

### Strings

- `str.hpp` (namespace `sihd::util::str`):
  - Split and join: `split`, `split_pair`, `join`, `trim`, `ltrim`, `rtrim`, `starts_with`, `ends_with`, `replace`, `iequals`, `to_upper`, `to_lower`.
  - Regex and glob: `regex_match`, `regex_replace`, `regex_filter`, `glob_match`, `glob_filter`.
  - Conversions: `to_bool`, `to_signed`, `to_unsigned`, `to_float`, `to_double`, `to_hex`, `to_dec`, `to_oct`, `to_base64`, `from_base64`, `convert_from_string<T>`, each returning `Result`.
  - Formatting: `format`, `format_time`, `format_localtime`, `bytes_str`, `hexdump`, `hexdump_fmt`, `word_wrap`, `wrap`, `demangle`, `demangle_type_name`.
  - Escapes and enclosures: `unquote`, `remove_escape_char`, `remove_enclosing`, `find_char_not_escaped`.
  - Search and output: `search` (matches a selection string against a list), `print`, `println`, `generate_random`, `generate_random_printable`.
- `Url`: parsed URL components (`scheme`, `userinfo`, and others) with `decode`, `encode`, `str_encode`, `str_decode`.
- `Regex`: compiled PCRE2 pattern (`compile` with `Mode`, `matches`, `find_all`, `replace_all`) returning `Result`.
- `mime.hpp`: MIME type constants and matching (`compatible`, `match_offered`, `best_match`).
- `locale.hpp`: `create_locale` and `normalize_locale_name` for a `std::locale` from a name.
- `LoadingBar`, `LoadingBarConfiguration`: terminal progress bar (`add_progress`, `print`).

### Errors

- `Error`: error with an `ErrorCode` and a message; `ErrorResolver<T>` maps native error types (`std::errc`, `std::error_code`) to `ErrorCode`; `error_errno` converts `errno`.
- `ErrorCode`: `none`, `unknown`, `invalid_argument`, `not_found`, `permission_denied`, `timeout`, `would_block`, `interrupted`, `out_of_memory`, `io_error`, `not_supported`, `already_exists`, `not_initialized`, `closed`, `overflow`.
- `Result<T>`: alias of `std::expected<T, Error>`. Used by most fallible APIs; `Result<void>` for success or error only.

### Statistics and profiling

- `Stat<T>`: running min, max, sum, variance, standard deviation, average, median, p95 and p99 (`add_sample`, `clear`).
- `PSquareEstimator`, `PSquareStat<T>`: streaming quantile estimates (P-square algorithm) for the median and p95/p99.
- `Timeit`: scope timer that logs its elapsed time on destruction; `elapsed()` reads it earlier.
- `Perf`: accumulates timing samples into a `Stat` (`begin`, `end`, `log`, `stat`, nested `Guard`).

### Misc

- `Random`: seeded generator (`frand`, `drand`); `num` namespace: `rand`, `frand`, `drand`, `seed`, `size`, `near`, `add_no_overflow`, `substract_no_overflow`.
- `Defer`: runs a callback on scope exit (`cancel`).
- `ScopedModifier<T>`: sets a value for a scope and restores the old value at its end.
- `AtExit`: `atexit::add_handler`, `remove_handler`, `clear_handlers`, `install`; runs `IRunnable` handlers at process exit.
- `endian.hpp`: `convert`, `convert_from`, `swap`, `read_le`, `read_be`, `switch_buffer_endianness`.
- `hash.hpp`: `hash::sha1`.
- `type.hpp`: `Type` enum and `type::from<T>()`, `type::str`, `type::size`, `type::is_unsigned`.
- `traits.hpp`: concepts (`TriviallyCopyable`, `Integral`, `Iterable`, `Erasable`, and others) and type traits.
- `tools.hpp`: boolean count helpers (`count_truth`, `only_one_true`, `maximum_one_true`, `only_one_false`, `maximum_one_false`).
- `thread.hpp`: `thread::set_name`, `name`, `id_str`, `equals`.
- `build.hpp`: compile-time platform and configuration flags in namespace `build` (`is_linux`, `is_windows`, `is_android`, `is_emscripten`, `is_run_with_asan`, `is_run_with_tsan`).
- `macro.hpp`: `SIHD_DIE_NE`, `SIHD_DIE_FALSE` (abort-on-mismatch checks).
- `version.hpp`: `SIHD_VERSION_STRING`, `SIHD_VERSION_NUM`.
- `fmt.hpp`: `fmt::formatter` specializations for `Array` and `ArrayView`.
- `fwd.hpp`: forward declarations of the main util types. `util.hpp` includes the whole module.

## Example

```cpp
#include <sihd/util/Callback.hpp>

using namespace sihd::util;

namespace
{
int counter = 0;

void void_args(int a)
{
    counter += a;
}

int ret_args(int a, int b)
{
    return a + b;
}
} // namespace

int main()
{
    CallbackManager cbm;
    cbm.set("test_void_args", &void_args);
    cbm.set("test_ret_args", &ret_args);

    cbm.call("test_void_args", 3);
    // counter == 3

    // the full signature is needed when all argument types are the same
    int res = cbm.call<int, int, int>("test_ret_args", 3, 4);
    // res == 7

    // an unknown name throws std::out_of_range
    return res == 7 ? 0 : 1;
}
```

Adapted from `modules/util/test/TestCallback.cpp`.

## Scripting

Exposed to Lua (`LuaUtilApi`, built with `lua=1`) and to Python (`sihd.util`, built with `py=1`).

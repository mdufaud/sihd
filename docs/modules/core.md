# core

Device and channel layer of SIHD. `core` builds a tree of named devices (`Named` → `Node` → `Device`) that own typed channels (`Channel`, a typed array bus with observers) and run through a service lifecycle: `setup` → `init` → `start` → `stop` → `reset`. It also ships ready-made devices for timing, sampling, filtering, message mapping, thread hand-over, batching, statistics, and recording and playback of channel values.

| | |
|---|---|
| Depends on | `util` |
| External libraries | none |
| Platforms | all |
| Build | `make m=core` |
| Header | `#include <sihd/core/Core.hpp>` |

## Capabilities

### Tree and lifecycle

- `Device`: base of all devices. Derives from `AChannelContainer` and `ACoreService`. Override the hooks `on_setup()`, `on_init()`, `on_start()`, `on_stop()`, `on_reset()` (not the `do_*` methods). `device_state()` and `device_state_str()` report the service state.
- `Core`: root container. Only cascades service transitions to its children; `is_running()` reports its state. Default name `"core"`.
- `AChannelContainer`: owns channels as children. `add_channel` / `add_channel_resizable` create a channel as a child; `add_unlinked_channel` / `add_unlinked_channel_resizable` declare a channel resolved against a linked channel at start: the declaration is the port, created as an owned channel when the name is not linked, checked against the linked channel otherwise (type and size, or type and a capacity at least as large for a resizable port). `get_channel` / `find_channel` return `Result<Channel *>`, `linked_channels` the channels reached through a link that is not a declared port (dynamic inputs, once started); `observe_channel` and `remove_channels_observation` manage observation.
- `ACoreService`: `AService` with a `ServiceController` handler. Base for lifecycle-driven objects.
- `ACoreObject`: `Named`, `Configurable`, channel handler and `ACoreService` for non-device core objects (for example `MemRecorder`).
- `fwd.hpp`: forward declarations of the core classes.

### Channels

- `Channel`: named typed array, built from a `sihd::util::Type` or type name plus a size, or from `Channel::build("name=...;type=...;size=...")`. `write(idx, value)` and `write(ArrByteView)` store then notify observers synchronously in the writing thread. `read<T>`, `read_into`, `value_at`, `copy_to` (value and timestamp in one atomic snapshot, into an array already sized), `timestamp`, `notify`. Resizing: `set_resizable`, `reserve`, `resize`; `write(ArrByteView)` gives a resizable channel the size of the written data, `write(ArrByteView, byte_offset)` and `write(idx, value)` only grow it. `set_write_on_change` skips writes that do not change the data. A write issued from inside a notification of the same channel is refused.
- `ChannelMatch`: comparison of a channel value at an index (`Equal`, `Superior`, `SuperiorEqual`, `Inferior`, `InferiorEqual`, `ByteAnd`, `ByteOr`, `ByteXor`). `match(channel)`, `verify(channel)`, `parse("cmp=equal;idx=0;value=false;invert=true")`, and factories `equal`, `superior`, `inferior`, and so on.
- `ChannelWaiter`: `ObserverWaiter<Channel>`. Blocks until a channel is notified: `wait_for_nb(duration, count)`, `prev_wait_for(duration)`, `notifications()`.
- `Records.hpp`: `PlayableRecord` (name, timestamp, array) and the containers used by recording and playback (`RecordedValue`, `MapListRecordedValues`, `SortedRecordedValues`).

### Recording and playback

- `DevRecorder`: records channel values, keyed by alias, to a handler set by path (`set_handler`). `add_record_channel("ALIAS=CHANNEL")`, `remove_recorded_channel`.
- `MemRecorder`: in-memory store that is both a record handler and a record provider. `add_record`, `add_records`, `provide`, `sorted_recorded_values`, `make_recorded_values`, `hexdump_records`, `clear`.
- `DevPlayer`: plays records from a provider onto channels on schedule. `set_provider(path)`, `set_provider_wait_time`, `add_alias(conf)`, `set_scheduler_queue_size`.

### Timing

- `DevPulsation`: heartbeat source. `set_frequency(hz)`; writing `true` to channel `activate` starts the `heartbeat` counter channel, writing `false` pauses it.
- `DevSampler`: copies an input channel to an output channel at a fixed frequency. `set_frequency(hz)`, `set_sample("..out=..in")`.

### Transformation

- `DevFilter`: rule-based writes from an input channel to an output channel when a `ChannelMatch` fires, with an optional delay. Rules are built with `DevFilter::Rule` (`in`, `out`, `trigger`, `write`, `write_same`, `match`, `delay`) and installed with `set_filter`, or parsed from a string with `set_filter_equal` (and the `superior`, `inferior`, `byte_and`, and related variants).
- `DevMessage`: maps channels to fields of an `IMessageField` message path. `set_message_path(path)`, `set_trigger_mode(bool)` (compute output only on trigger instead of immediately).

### Threading and data flow

A device is wired like a blueprint: its ports are channels and other devices alias them with `add_link`. Two kinds of devices:

- Fixed ports (the net devices: `tx` in, `rx` out): the device declares its channels at `init()`; a declared name the caller links is checked at `start()` against the declaration.
- Dynamic inputs (`DevWorker`, `DevWorkerPool`, `DevBatcher`, `DevStats`): every channel the caller links is an input, whatever its name, type or size. At its first `start()` the device creates its outputs after each input `NAME` (`NAME_out`, `NAME_batch`, `NAME_count`...), from the type and size of that input, so devices that alias these outputs start after it. A link to something that is not a channel makes `start()` fail.

Several devices can alias the same channel and each one observes it, so a stream fans out without copy. The owner of a channel chooses its `write_on_change`: the event outputs below notify every write, a linked channel keeps its owner's choice (a fd or a value equal to the previous one is then not notified).

- `DevWorker`: thread boundary, on one thread. Every write of a linked channel `NAME` is queued (`queue_size` entries, default 1024) and rewritten on `NAME_out` (same type, size and capacity) by the device thread, in order. `NAME_out` is stamped when the worker writes it, so `out.timestamp() - in.timestamp()` is the latency of the hand-over. When the queue is full the newest value is dropped and the `dropped` counter channel is incremented. `stop()` writes what is still queued.
- `DevWorkerPool`: a `DevWorker` on `threads` threads (default: the hardware concurrency), with a queue of `queue_size` entries per input. Each input keeps its order and a busy input does not hold a thread while the others wait.
- `DevBatcher`: concatenates `samples` writes of each linked channel `NAME` into one block written on `NAME_batch` (its type, a capacity of `samples` times its size, or its capacity when resizable). A block takes the size of its data and notifies even when it equals the previous one. Blocks are written every `period` milliseconds (default 100); a partial block is written once it is a full period old, and on `stop()`.
- `DevStats`: for each linked channel `NAME`, every `period` milliseconds (default 1000) and on `stop()`, publishes the totals since `start()`: `NAME_count` and `NAME_rate` (writes and writes per second), `NAME_bytes` and `NAME_byte_rate`, and `NAME_min`, `NAME_max`, `NAME_mean` of the elements for numeric types (zero for `bool`, `char`, `byte` and `ubyte`). `NAME_count` is written last.

### Profiling

- `TreeProfiler`: strace-like tracer of channel notifications and service operations. `observe(Node *)`, `observe(Channel *)`, `observe(AService *)`, `start_when` / `stop_when` for a capture window, `start_capture`, `stop_capture`, `report_str`, `log_report`, `events`, `add_observer`, `set_trace`.

## Example

```cpp
#include <chrono>

#include <sihd/core/ChannelWaiter.hpp>
#include <sihd/core/Core.hpp>
#include <sihd/core/DevPulsation.hpp>

using namespace sihd::core;

bool run_pulsation()
{
    Core core;

    DevPulsation *pulse = core.add_child<DevPulsation>("pulsation");
    if (!pulse->set_conf("frequency", 1000.0))
        return false;
    if (!core.init() || !core.start())
        return false;

    Channel *activate = pulse->get_channel("activate").value_or(nullptr);
    Channel *beat = pulse->get_channel("heartbeat").value_or(nullptr);
    if (activate == nullptr || beat == nullptr)
        return false;

    bool beaten = false;
    {
        ChannelWaiter waiter(beat);
        activate->write(0, true);
        beaten = waiter.wait_for_nb(std::chrono::milliseconds(1000), 4);
    }

    return core.stop() && core.reset() && beaten;
}
```

Adapted from `modules/core/test/TestDevPulsation.cpp`.

### Handing a stream to another thread

```cpp
#include <sihd/core/Core.hpp>
#include <sihd/core/DevWorker.hpp>
#include <sihd/core/DevStats.hpp>

using namespace sihd::core;

bool relay_and_count()
{
    Core core;
    Channel *source = core.add_channel("source", "int", 1);

    DevWorker *worker = core.add_child<DevWorker>("worker");
    DevStats *stats = core.add_child<DevStats>("stats");
    if (!worker->add_link("source", "..source"))
        return false;
    if (!stats->add_link("source", "..source") || !stats->set_conf_int("period", 100))
        return false;
    if (!core.init() || !core.start())
        return false;

    Channel *relayed = worker->get_channel("source_out").value_or(nullptr);
    const bool written = source->write<int>(0, 42);
    const bool stopped = core.stop();

    // stop() drained the queue: source_out holds 42, stamped when the worker wrote it
    return relayed != nullptr && written && stopped && relayed->read<int>(0) == 42
           && relayed->timestamp() >= source->timestamp();
}
```

Adapted from `modules/core/test/TestDevWorker.cpp` and `modules/core/test/TestDevStats.cpp`.

## Scripting

Exposed to Lua and Python.

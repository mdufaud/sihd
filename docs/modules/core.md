# core

Device and channel layer of SIHD. `core` builds a tree of named devices (`Named` → `Node` → `Device`) that own typed channels (`Channel`, a typed array bus with observers) and run through a service lifecycle: `setup` → `init` → `start` → `stop` → `reset`. It also ships ready-made devices for timing, sampling, filtering, message mapping, and recording and playback of channel values.

| | |
|---|---|
| Depends on | `util`, `sys` |
| External libraries | none |
| Platforms | all |
| Build | `make m=core` |
| Header | `#include <sihd/core/Core.hpp>` |

## Capabilities

### Tree and lifecycle

- `Device`: base of all devices. Derives from `AChannelContainer` and `ACoreService`. Override the hooks `on_setup()`, `on_init()`, `on_start()`, `on_stop()`, `on_reset()` (not the `do_*` methods). `device_state()` and `device_state_str()` report the service state.
- `Core`: root container. Only cascades service transitions to its children; `is_running()` reports its state. Default name `"core"`.
- `AChannelContainer`: owns channels as children. `add_channel` / `add_channel_resizable` create a channel as a child; `add_unlinked_channel` / `add_unlinked_channel_resizable` declare a channel resolved against a linked channel at start. `get_channel` / `find_channel` return `Result<Channel *>`; `observe_channel` and `remove_channels_observation` manage observation.
- `ACoreService`: `AService` with a `ServiceController` handler. Base for lifecycle-driven objects.
- `ACoreObject`: `Named`, `Configurable`, channel handler and `ACoreService` for non-device core objects (for example `MemRecorder`).
- `fwd.hpp`: forward declarations of the core classes.

### Channels

- `Channel`: named typed array, built from a `sihd::util::Type` or type name plus a size, or from `Channel::build("name=...;type=...;size=...")`. `write(idx, value)` and `write(ArrByteView)` store then notify observers synchronously in the writing thread. `read<T>`, `read_into`, `value_at`, `copy_to`, `timestamp`, `notify`. Resizing: `set_resizable`, `reserve`, `resize`. `set_write_on_change` skips writes that do not change the data. A write issued from inside a notification of the same channel is refused.
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

## Scripting

Exposed to Lua and Python.

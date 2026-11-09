# tui

Terminal user interface components built on FTXUI. `CliComponent` is an interactive command prompt with output history, `LoggerComponent` is a live view of the `sihd::util` log stream with a filter bar, `TableComponent` and `MessageTableComponent` are sortable tables, `CardGridComponent` lays out selectable cards, `DevScreen` with `Refresher` redraws a screen when `core` channels change, and `TreeMonitorComponent` with `TreeTraceComponent` show a `core` tree live from a `TreeMonitor`. The components return `ftxui::Component` values that can be embedded in any FTXUI layout.

| | |
|---|---|
| Depends on | `sys` |
| External libraries | ftxui |
| Platforms | all |
| Build | `make m=tui` |
| Header | `#include <sihd/tui/CliComponent.hpp>`, `#include <sihd/tui/LoggerComponent.hpp>`, `#include <sihd/tui/TableComponent.hpp>`, `#include <sihd/tui/MessageTableComponent.hpp>`, `#include <sihd/tui/CardGridComponent.hpp>`, `#include <sihd/tui/Refresher.hpp>`, `#include <sihd/tui/DevScreen.hpp>`, `#include <sihd/tui/channel_view.hpp>`, `#include <sihd/tui/TreeMonitorComponent.hpp>`, `#include <sihd/tui/TreeTraceComponent.hpp>` (umbrella header: `#include <sihd/tui.hpp>`) |

## Capabilities

### Command prompt

- `CliComponent(on_input, options)`: builds a prompt component. `on_input` receives the `Cli *` interface and each submitted line. The returned component also implements `Cli`, so it can be cast with `dynamic_cast<Cli &>` to push output from the application.
- `CliOptions`: `max_lines` (output history size), `scroll_to_last_output`, `prompt`, `placeholder`, `completions` (`CompletionMap`: command name to list of argument candidates).
- `Cli`: `add_output(line, style)`, `add_output(line)` (normal style), `clear_output`.
- `OutputStyle`: `normal`, `info`, `success`, `warning`, `error`.
- Keyboard: Tab and Shift+Tab cycle completions, Escape cancels them, Up and Down browse input history, Ctrl+L clears the output.

### Log viewer

- `LoggerComponent(options)`: builds a component that displays log lines. On construction it registers itself as a logger with `sihd::util::LoggerManager`, so existing `SIHD_LOG` calls appear in it.
- `LoggerOptions`: `max_logs` (history size), `scroll_to_last_log`, `add_bar` (show the filter input line).

### Tables

- `TableComponent(options)`: a table whose rows come from `TableOptions::rows`, called at each frame (one cell per column of `columns`). Up, Down, PageUp, PageDown, Home and End select a row, Left and Right pick the sorted column (`sort_column`), `r` reverses the order (`sort_descending`). Cells that parse as numbers sort as numbers, before text.
- `MessageTableComponent(records, schema, options)`: a `TableComponent` over a channel holding records of a `sihd::util::Message` schema back to back, one row per record. `MessageTableOptions`: `fields` (shown in that order, all when empty), `titles` (header per field), `formatters` (text per field), `sort_column`, `sort_descending`. `field_str(field)` is the default text: a `char` field up to its first NUL, a `ubyte` array as hex bytes joined by `:`, values otherwise.
- `CardGridComponent(options)`: cards from `CardGridOptions::cards`, called at each frame, laid out left to right and wrapped to the width (`card_width` columns per card), each in a titled frame. The selection follows `Card::key` across frames and is kept in `selected` (an `ftxui::Ref<std::string>`, so the caller can read it to show details), it is empty while the selection follows the first card, which happens when the key is unknown or no longer among the cards. Left and Right move by one card, Up and Down by one row, Home and End jump to the ends; the selected card has a double frame and stays scrolled into view.

### Channels on screen

- `Refresher(post)`: coalesces redraw requests from any thread. `request()` calls `post` (for a screen, `screen.PostEvent(ftxui::Event::Custom)`) only when no frame is pending; `rendered()` marks a frame drawn. `RefreshOnRender(component, refresher)` calls `rendered()` each time `component` is drawn.
- `DevScreen`: a `core` device whose linked channels are all inputs, whatever their name; a write of any of them calls `request()` on the refresher given by `set_refresher`. Link a heartbeat (`DevPulsation`) for a steady rate, or the data channels for a redraw on change.
- `channel_str(channel)` / `channel_text(channel)`: every element separated by a space, a `char` channel as text up to its first NUL, read under the channel lock. `array_str(array)`: the same text from a copy, such as the data of a `TreeMonitorEvent`. `channel_gauge(channel, min, max)`: the first element as a gauge. `ChannelSparkline(channel, samples)`: the first element sampled at each frame, the last `samples` drawn as bars; a NaN or infinite value is not sampled.

### Device tree on screen

- `TreeMonitorComponent(monitor, refresher)`: the tree of a `core::TreeMonitor`, one line per node: service state in color, channel value, write count. It subscribes to the monitor and calls `request()` on the refresher at every event that changes a line, so the screen redraws on each change with no polling; a detached object loses its line. Built after `observe`, it starts from a walk of the tree; built before, the attached events fill it. Up and Down select a line, Left folds a node (or goes to its parent), Right unfolds it.
- `TreeTraceComponent(monitor, refresher, max_events)`: a `TableComponent` of the `max_events` most recent monitor events (at least one), newest first: attaches, detaches, writes with their value, write durations, operations with the service state.
- The monitor and the refresher outlive the components. `modules/tui/demo/tui_tree_monitor.cpp` shows a pulsing device and its statistics live.

```cpp
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <sihd/core/Core.hpp>
#include <sihd/core/TreeMonitor.hpp>
#include <sihd/tui/Refresher.hpp>
#include <sihd/tui/TreeMonitorComponent.hpp>
#include <sihd/tui/TreeTraceComponent.hpp>

using namespace sihd::core;
using namespace sihd::tui;
using namespace ftxui;

void show(Core & core)
{
    TreeMonitor monitor;
    auto screen = ScreenInteractive::Fullscreen();
    Refresher refresher([&screen] { screen.PostEvent(Event::Custom); });
    Component tree = TreeMonitorComponent(monitor, refresher);
    Component trace = TreeTraceComponent(monitor, refresher, 500);
    sihd::util::Node & root = core;
    monitor.observe(&root);

    Component layout = Container::Horizontal({tree, trace});
    Component view = Renderer(layout, [&] {
        return hbox({window(text("tree"), tree->Render()) | flex, window(text("trace"), trace->Render()) | flex});
    });
    screen.Loop(RefreshOnRender(view, refresher));
}
```

Adapted from `modules/tui/demo/tui_tree_monitor.cpp`.

## Example

```cpp
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <sihd/tui/CliComponent.hpp>

using namespace sihd::tui;
using namespace ftxui;

int main()
{
    CompletionMap completions = {
        {"help", {}},
        {"quit", {}},
        {"get", {"name", "version", "status", "config"}},
    };

    auto on_input = [](Cli *cli, std::string_view input) {
        if (input == "help")
            cli->add_output("Available commands: help, quit, get", OutputStyle::info);
        else if (input.starts_with("get"))
            cli->add_output(std::string("executed: ") + std::string(input), OutputStyle::success);
        else
            cli->add_output(std::string("unknown command: ") + std::string(input), OutputStyle::error);
    };

    Component cli = CliComponent(std::move(on_input),
                                 CliOptions {
                                     .max_lines = 200,
                                     .completions = std::move(completions),
                                 });
    auto renderer = Renderer(cli, [&] {
        return window(text("cli") | hcenter | bold, cli->Render()) | size(WIDTH, EQUAL, 80) | size(HEIGHT, EQUAL, 20);
    });

    auto screen = ScreenInteractive::FitComponent();
    screen.Loop(renderer);
    return 0;
}
```

Adapted from `modules/tui/test/TestCli.cpp`.

## Scripting

Not exposed to Lua or Python.

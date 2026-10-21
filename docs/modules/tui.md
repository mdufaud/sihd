# tui

Terminal user interface components built on FTXUI. `CliComponent` is an interactive command prompt with output history, and `LoggerComponent` is a live view of the `sihd::util` log stream with a filter bar. Both return `ftxui::Component` values that can be embedded in any FTXUI layout.

| | |
|---|---|
| Depends on | `util`, `sys` |
| External libraries | ftxui |
| Platforms | all |
| Build | `make m=tui` |
| Header | `#include <sihd/tui/CliComponent.hpp>`, `#include <sihd/tui/LoggerComponent.hpp>` (umbrella header: `#include <sihd/tui.hpp>`) |

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

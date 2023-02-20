#include <sihd/tui/LoggerWindow.hpp>
#include <sihd/util/Logger.hpp>

#include <ftxui/component/captured_mouse.hpp> // for ftxui
#include <ftxui/component/component.hpp>      // for CatchEvent, Renderer
#include <ftxui/component/event.hpp>          // for Event
#include <ftxui/component/mouse.hpp> // for Mouse, Mouse::Left, Mouse::Middle, Mouse::None, Mouse::Pressed, Mouse::Released, Mouse::Right, Mouse::WheelDown, Mouse::WheelUp
#include <ftxui/component/screen_interactive.hpp> // for ScreenInteractive
#include <ftxui/dom/elements.hpp>                 // for text, vbox, window, Element, Elements

namespace sihd::tui
{

using namespace ftxui;

LoggerWindow::LoggerWindow(size_t max_logs): _max_logs(max_logs) {}

LoggerWindow::~LoggerWindow() {}

ftxui::Element LoggerWindow::create(ftxui::Element title)
{
    Elements logs;
    logs.reserve(_logs.size());
    for (const auto & log : _logs)
    {
        logs.emplace_back(text(log));
    }
    return window(std::move(title), vbox(std::move(logs)));
}

void LoggerWindow::log(const sihd::util::LogInfo & info, std::string_view msg)
{
    const char *level;
    switch (info.level)
    {
        case sihd::util::LogLevel::emergency:
            level = "EMER";
            break;
        case sihd::util::LogLevel::alert:
            level = "A";
            break;
        case sihd::util::LogLevel::critical:
            level = "C";
            break;
        case sihd::util::LogLevel::error:
            level = "E";
            break;
        case sihd::util::LogLevel::warning:
            level = "W";
            break;
        case sihd::util::LogLevel::notice:
            level = "N";
            break;
        case sihd::util::LogLevel::info:
            level = "I";
            break;
        case sihd::util::LogLevel::debug:
            level = "D";
            break;
        default:
            level = "-";
    }
    if (_logs.size() >= _max_logs)
        _logs.pop_front();
    _logs.emplace_back(
        fmt::format("{} [{}] <{}> {}\n", level, info.thread_name.data(), info.source.data(), msg.data()));
}

} // namespace sihd::tui
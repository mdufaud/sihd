#include "ftxui/component/captured_mouse.hpp" // for ftxui
#include "ftxui/component/component.hpp"      // for CatchEvent, Renderer
#include "ftxui/component/event.hpp"          // for Event
#include "ftxui/component/mouse.hpp" // for Mouse, Mouse::Left, Mouse::Middle, Mouse::None, Mouse::Pressed, Mouse::Released, Mouse::Right, Mouse::WheelDown, Mouse::WheelUp
#include "ftxui/component/screen_interactive.hpp" // for ScreenInteractive
#include "ftxui/dom/elements.hpp"                 // for text, vbox, window, Element, Elements

#include <sihd/tui/LoggerWindow.hpp>
#include <sihd/util/Logger.hpp>

using namespace ftxui;

static std::string Stringify(Event event)
{
    std::string out;
    for (auto & it : event.input())
        out += " " + std::to_string((unsigned int)it);

    out = "(" + out + " ) -> ";
    if (event.is_character())
    {
        out += "character(" + event.character() + ")";
    }
    else if (event.is_mouse())
    {
        out += "mouse";
        switch (event.mouse().button)
        {
            case Mouse::Left:
                out += "_left";
                break;
            case Mouse::Middle:
                out += "_middle";
                break;
            case Mouse::Right:
                out += "_right";
                break;
            case Mouse::None:
                out += "_none";
                break;
            case Mouse::WheelUp:
                out += "_wheel_up";
                break;
            case Mouse::WheelDown:
                out += "_wheel_down";
                break;
        }
        switch (event.mouse().motion)
        {
            case Mouse::Pressed:
                out += "_pressed";
                break;
            case Mouse::Released:
                out += "_released";
                break;
        }
        if (event.mouse().control)
            out += "_control";
        if (event.mouse().shift)
            out += "_shift";
        if (event.mouse().meta)
            out += "_meta";

        out += "(" + //
               std::to_string(event.mouse().x) + "," + std::to_string(event.mouse().y) + ")";
    }
    else
    {
        out += "(special)";
    }
    return out;
}

int main()
{
    using namespace sihd::util;
    using namespace sihd::tui;

    Logger logger("tui-demo");
    LoggerWindow logger_win(13);
    TmpLogger tmp_logger(&logger_win);

    auto screen = ftxui::ScreenInteractive::TerminalOutput();

    auto component = ftxui::Renderer([&] {
        return logger_win.create(ftxui::text("logger") | hcenter | bold) | size(WIDTH, EQUAL, 100)
               | size(HEIGHT, EQUAL, 15);
        // | bgcolor(Color::HSV(1 * 25, 255, 255))
        // | color(Color::Black);
    });

    component |= ftxui::CatchEvent([&](ftxui::Event event) {
        logger.info(Stringify(event));
        return true;
    });

    screen.Loop(component);
    return 0;
}
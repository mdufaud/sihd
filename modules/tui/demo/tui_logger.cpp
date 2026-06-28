#include "ftxui/component/captured_mouse.hpp" // for ftxui
#include "ftxui/component/component.hpp"      // for CatchEvent, Renderer
#include "ftxui/component/event.hpp"          // for Event
#include "ftxui/component/mouse.hpp" // for Mouse, Mouse::Left, Mouse::Middle, Mouse::None, Mouse::Pressed, Mouse::Released, Mouse::Right, Mouse::WheelDown, Mouse::WheelUp
#include "ftxui/component/screen_interactive.hpp" // for ScreenInteractive
#include "ftxui/dom/elements.hpp"                 // for text, vbox, window, Element, Elements

#include <sihd/sys/os.hpp>
#include <sihd/tui/LoggerComponent.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/num.hpp>
#include <sihd/util/str.hpp>

using namespace sihd::util;
using namespace sihd::sys;
using namespace sihd::tui;
using namespace ftxui;

Logger logger("tui-demo");

void randomize_log()
{
    auto rand = num::rand(0, 7);
    switch (rand)
    {
        case 0:
            logger.emergency("EMERGENCY !!!");
            break;
        case 1:
            logger.alert("ALERT !!");
            break;
        case 2:
            logger.critical("CRITICAL !");
            break;
        case 3:
            logger.error("Donec at cursus tellus, quis consequat lacus");
            break;
        case 4:
            logger.warning(
                "Maecenas non fermentum arcu, non finibus lectus. Vestibulum quis viverra leo. Aliquam pellentesque sagittis turpis non scelerisque");
            break;
        case 5:
            logger.notice(
                "Quisque malesuada eros quis imperdiet molestie. Curabitur dui nunc, lacinia nec molestie vitae, vulputate vitae ligula");
            break;
        case 6:
            logger.info(fmt::format("Current memory: {}", str::bytes_str(sihd::sys::os::current_rss())));
            break;
        case 7:
            logger.debug("Lorem ipsum dolor sit amet, consectetur adipiscing elit");
            break;
    }
}

int main()
{
    auto container = LoggerComponent(LoggerOptions {
        .max_logs = 200,
        .scroll_to_last_log = true,
    });

    container |= ftxui::CatchEvent([&](ftxui::Event event) {
        if (event.character() == "L")
        {
            randomize_log();
            return true;
        }
        return false;
    });

    auto renderer = ftxui::Renderer(container, [&] {
        return window(text("logger") | hcenter | bold, container->Render()) | size(WIDTH, EQUAL, 100)
               | size(HEIGHT, EQUAL, 20);
    });

    logger.info("Press 'L' to generate logs");

    auto screen = ftxui::ScreenInteractive::FitComponent();
    screen.Loop(renderer);
    return 0;
}
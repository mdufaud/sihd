#include <gtest/gtest.h>
#include <iostream>

#include "ftxui/component/captured_mouse.hpp" // for ftxui
#include "ftxui/component/component.hpp"      // for CatchEvent, Renderer
#include "ftxui/component/event.hpp"          // for Event
#include "ftxui/component/mouse.hpp" // for Mouse, Mouse::Left, Mouse::Middle, Mouse::None, Mouse::Pressed, Mouse::Released, Mouse::Right, Mouse::WheelDown, Mouse::WheelUp
#include "ftxui/component/screen_interactive.hpp" // for ScreenInteractive
#include "ftxui/dom/elements.hpp"                 // for text, vbox, window, Element, Elements

#include <sihd/tui/LoggerComponent.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/num.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/term.hpp>

namespace test
{

SIHD_NEW_LOGGER("test");

using namespace sihd::util;
using namespace sihd::tui;
using namespace ftxui;

void randomize_log()
{
    auto rand = num::rand(0, 7);
    switch (rand)
    {
        case 0:
            SIHD_LOG(emergency, "EMERGENCY !!!");
            break;
        case 1:
            SIHD_LOG(alert, "ALERT !!");
            break;
        case 2:
            SIHD_LOG(critical, "CRITICAL !");
            break;
        case 3:
            SIHD_LOG(error, "Donec at cursus tellus, quis consequat lacus");
            break;
        case 4:
            SIHD_LOG(
                warning,
                "Maecenas non fermentum arcu, non finibus lectus. Vestibulum quis viverra leo. Aliquam pellentesque sagittis turpis non scelerisque");
            break;
        case 5:
            SIHD_LOG(
                notice,
                "Quisque malesuada eros quis imperdiet molestie. Curabitur dui nunc, lacinia nec molestie vitae, vulputate vitae ligula");
            break;
        case 6:
            SIHD_LOG(info, "Current memory: {}", str::bytes_str(sihd::sys::os::current_rss()));
            break;
        case 7:
            SIHD_LOG(debug, "Lorem ipsum dolor sit amet, consectetur adipiscing elit");
            break;
    }
}

class TestLogger: public ::testing::Test
{
    protected:
        TestLogger() = default;

        virtual ~TestLogger() = default;

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestLogger, test_tui_cmd)
{
    if (sihd::util::term::is_interactive() == false)
        GTEST_SKIP_("requires interaction");

    using namespace sihd::util;
    using namespace sihd::tui;

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

    SIHD_LOG(info, "Press 'L' to generate logs");

    auto screen = ftxui::ScreenInteractive::FitComponent();
    screen.Loop(renderer);
}
} // namespace test
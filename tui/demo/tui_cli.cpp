#include "ftxui/component/component.hpp"          // for Make, Input
#include "ftxui/component/component_options.hpp"  // for InputOption
#include "ftxui/component/screen_interactive.hpp" // for ScreenInteractive
#include "ftxui/dom/elements.hpp"                 // for text, vbox, window, Element, Elements

#include <sihd/tui/CliComponent.hpp>
#include <sihd/util/Logger.hpp>

using namespace sihd::util;
using namespace sihd::tui;
using namespace ftxui;

int main()
{
    auto container = CliComponent(CliOptions {
        .max_lines = 200,
        .scroll_to_last_output = true,
    });

    auto renderer = ftxui::Renderer(container, [&] {
        return window(text("logger") | hcenter | bold, container->Render()) | size(WIDTH, EQUAL, 100)
               | size(HEIGHT, EQUAL, 20);
    });

    auto screen = ftxui::ScreenInteractive::FitComponent();
    screen.Loop(renderer);

    return 0;
}
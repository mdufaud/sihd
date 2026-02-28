#include <gtest/gtest.h>

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <sihd/tui/CliComponent.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/term.hpp>

namespace test
{

SIHD_LOGGER;

using namespace sihd::util;
using namespace sihd::tui;
using namespace ftxui;

class TestCli: public ::testing::Test
{
    protected:
        TestCli() = default;
        virtual ~TestCli() = default;
        virtual void SetUp() {}
        virtual void TearDown() {}
};

TEST_F(TestCli, test_tui_cli)
{
    if (sihd::util::term::is_interactive() == false)
        GTEST_SKIP_("requires interaction");

    CompletionMap completions = {
        {"help", {}},
        {"quit", {}},
        {"get", {"name", "version", "status", "config"}},
        {"set", {"name", "verbose", "timeout"}},
        {"show", {"logs", "devices", "channels", "stats"}},
        {"start", {"service", "device"}},
        {"stop", {"service", "device"}},
    };

    ftxui::Component cli;

    auto cli_callback = [](Cli *cli, std::string_view input) {
        if (input == "help")
        {
            cli->add_output("Available commands: help, quit, get, set, show, start, stop");
            cli->add_output("Use Tab to autocomplete");
        }
        else if (input == "quit")
        {
            cli->add_output("Bye!");
        }
        else
        {
            cli->add_output(std::string("executed: ") + std::string(input));
        }
    };

    cli = CliComponent(
        std::move(cli_callback),
        CliOptions {
            .max_lines = 200,
            .scroll_to_last_output = true,
            .completions = std::move(completions),
        });

    dynamic_cast<Cli &>(*cli).add_output("Type 'help' for available commands. Tab for completion. 'q' to quit.");

    auto renderer = ftxui::Renderer(cli, [&] {
        return window(text("cli") | hcenter | bold, cli->Render()) | size(WIDTH, EQUAL, 80)
               | size(HEIGHT, EQUAL, 20);
    });

    renderer |= ftxui::CatchEvent([&](ftxui::Event event) {
        if (event == Event::Character('q') && !cli->Focused())
            return true;
        return false;
    });

    auto screen = ftxui::ScreenInteractive::FitComponent();
    screen.Loop(renderer);
}

} // namespace test

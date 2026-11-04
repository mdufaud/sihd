#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <sihd/core/Core.hpp>
#include <sihd/core/DevPulsation.hpp>
#include <sihd/core/DevStats.hpp>
#include <sihd/core/TreeMonitor.hpp>
#include <sihd/sys/App.hpp>
#include <sihd/tui/Refresher.hpp>
#include <sihd/tui/TreeMonitorComponent.hpp>
#include <sihd/tui/TreeTraceComponent.hpp>

using namespace sihd::core;
using namespace sihd::sys;
using namespace sihd::tui;
using namespace ftxui;

int main(int argc, char **argv)
{
    App app({
        .name = "tui_tree_monitor",
        .description = "Live device tree demo",
    });

    app.root().on_run([&] {
        Core core("core");
        DevPulsation *pulse = core.add_child<DevPulsation>("pulse");
        pulse->set_frequency(4);
        DevStats *stats = core.add_child<DevStats>("stats");
        stats->add_link("beats", "..pulse.heartbeat");
        stats->set_period(1000);

        TreeMonitor monitor;
        auto screen = ScreenInteractive::Fullscreen();
        Refresher refresher([&screen] { screen.PostEvent(Event::Custom); });
        Component tree = TreeMonitorComponent(monitor, refresher);
        Component trace = TreeTraceComponent(monitor, refresher, 500);
        // Core is a Node and an AService: observe the tree
        using sihd::util::Node;
        Node & tree_root = core;
        monitor.observe(&tree_root);

        if (core.init() == false || core.start() == false)
            return;
        Channel *activate = pulse->get_channel("activate").value_or(nullptr);

        Component layout = Container::Horizontal({tree, trace});
        layout |= CatchEvent([&](Event event) {
            if (event == Event::Character('a'))
                return activate->write<bool>(0, !activate->read<bool>(0));
            if (event == Event::Character('s'))
                return core.is_running() ? core.stop() : core.start();
            if (event == Event::Character('q'))
            {
                screen.Exit();
                return true;
            }
            return false;
        });
        Component root = Renderer(layout, [&] {
            return vbox({
                hbox({
                    window(text("tree"), tree->Render()) | flex,
                    window(text("trace"), trace->Render()) | flex,
                }) | flex,
                text("a: pulse on/off  s: start/stop  tab: switch panel  q: quit") | dim,
            });
        });
        screen.Loop(RefreshOnRender(root, refresher));

        core.stop();
        monitor.reset();
    });

    return app.run(argc, argv);
}

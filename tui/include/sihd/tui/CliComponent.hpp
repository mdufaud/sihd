#ifndef __SIHD_TUI_CLICOMPONENT_HPP__
#define __SIHD_TUI_CLICOMPONENT_HPP__

#include <ftxui/component/component.hpp>

namespace sihd::tui
{

struct CliOptions
{
        size_t max_lines = 1024;
        bool scroll_to_last_output = true;
};

ftxui::Component CliComponent(std::function<void(std::string_view)> && on_input, CliOptions && options);

} // namespace sihd::tui

#endif

#ifndef __SIHD_TUI_CLICOMPONENT_HPP__
#define __SIHD_TUI_CLICOMPONENT_HPP__

#include <map>
#include <string>
#include <vector>

#include <ftxui/component/component.hpp>

namespace sihd::tui
{

// command name -> list of possible arguments
using CompletionMap = std::map<std::string, std::vector<std::string>>;

struct CliOptions
{
        size_t max_lines = 1024;
        bool scroll_to_last_output = true;
        CompletionMap completions = {};
};

class Cli
{
    public:
        virtual ~Cli() = default;
        virtual void add_output(std::string_view line) = 0;
};

ftxui::Component CliComponent(std::function<void(Cli *, std::string_view)> && on_input,
                              CliOptions && options);

} // namespace sihd::tui

#endif

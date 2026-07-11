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
        std::string prompt = "> ";
        std::string placeholder = "Type something... ";
        CompletionMap completions = {};
};

enum class OutputStyle
{
    normal,
    info,
    success,
    warning,
    error,
};

class Cli
{
    public:
        virtual ~Cli() = default;
        virtual void add_output(std::string_view line, OutputStyle style) = 0;
        void add_output(std::string_view line) { this->add_output(line, OutputStyle::normal); }
        virtual void clear_output() = 0;
};

ftxui::Component CliComponent(std::function<void(Cli *, std::string_view)> && on_input,
                              CliOptions && options);

} // namespace sihd::tui

#endif

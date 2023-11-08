#ifndef __SIHD_TUI_LOGGERCOMPONENT_HPP__
#define __SIHD_TUI_LOGGERCOMPONENT_HPP__

#include <ftxui/component/component.hpp>

namespace sihd::tui
{

struct LoggerOptions
{
        size_t max_logs = 1024;
        bool scroll_to_last_log = true;
        bool add_bar = true;
};

ftxui::Component LoggerComponent(LoggerOptions && options);

} // namespace sihd::tui

#endif
#ifndef __SIHD_TUI_TREEMONITORCOMPONENT_HPP__
#define __SIHD_TUI_TREEMONITORCOMPONENT_HPP__

#include <ftxui/component/component.hpp>

#include <sihd/core/TreeMonitor.hpp>
#include <sihd/tui/Refresher.hpp>

namespace sihd::tui
{

// the monitor and the refresher outlive the component
ftxui::Component TreeMonitorComponent(sihd::core::TreeMonitor & monitor, Refresher & refresher);

} // namespace sihd::tui

#endif

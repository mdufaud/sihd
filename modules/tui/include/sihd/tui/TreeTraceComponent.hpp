#ifndef __SIHD_TUI_TREETRACECOMPONENT_HPP__
#define __SIHD_TUI_TREETRACECOMPONENT_HPP__

#include <ftxui/component/component.hpp>

#include <sihd/core/TreeMonitor.hpp>
#include <sihd/tui/Refresher.hpp>

namespace sihd::tui
{

// the monitor and the refresher outlive the component
ftxui::Component TreeTraceComponent(sihd::core::TreeMonitor & monitor, Refresher & refresher, size_t max_events);

} // namespace sihd::tui

#endif

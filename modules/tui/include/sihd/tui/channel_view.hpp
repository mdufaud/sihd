#ifndef __SIHD_TUI_CHANNEL_VIEW_HPP__
#define __SIHD_TUI_CHANNEL_VIEW_HPP__

#include <string>

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include <sihd/core/Channel.hpp>
#include <sihd/util/IArray.hpp>

namespace sihd::tui
{

// a char channel reads as text up to its first NUL
std::string channel_str(const sihd::core::Channel & channel);
std::string array_str(const sihd::util::IArray & array);
ftxui::Element channel_text(const sihd::core::Channel & channel);
ftxui::Element channel_gauge(const sihd::core::Channel & channel, double min, double max);
// the first element sampled at each frame, the last `samples` drawn as bars; the channel outlives the component
ftxui::Component ChannelSparkline(const sihd::core::Channel & channel, size_t samples);

} // namespace sihd::tui

#endif

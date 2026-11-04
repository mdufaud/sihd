#ifndef __SIHD_TUI_CARDGRIDCOMPONENT_HPP__
#define __SIHD_TUI_CARDGRIDCOMPONENT_HPP__

#include <functional>
#include <string>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/util/ref.hpp>

namespace sihd::tui
{

struct Card
{
        // identifies the card across frames, the selection follows it
        std::string key;
        std::string title;
        ftxui::Element body;
};

struct CardGridOptions
{
        std::function<std::vector<Card>()> cards;
        int card_width = 32;
        // key of the selected card, empty while the selection follows the first card
        ftxui::Ref<std::string> selected;
};

ftxui::Component CardGridComponent(CardGridOptions && options);

} // namespace sihd::tui

#endif

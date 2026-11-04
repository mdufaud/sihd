#include <algorithm>

#include <ftxui/component/event.hpp>
#include <ftxui/screen/box.hpp>

#include <sihd/tui/CardGridComponent.hpp>

namespace sihd::tui
{

using namespace ftxui;

namespace
{

class CardGrid: public ComponentBase
{
    public:
        explicit CardGrid(CardGridOptions && options): _options(std::move(options)) {}

        Element OnRender() override
        {
            std::vector<Card> cards = _options.cards ? _options.cards() : std::vector<Card> {};
            _keys.clear();
            for (const Card & card : cards)
                _keys.push_back(card.key);
            const size_t selected = this->selected_index();
            if (selected == 0 && _keys.empty() == false && *_options.selected != _keys[0])
                *_options.selected = "";

            Elements items;
            for (size_t i = 0; i < cards.size(); ++i)
            {
                const bool is_selected = i == selected;
                Element title = text(" " + cards[i].title + " ");
                if (is_selected)
                    title = title | inverted;
                Element item = window(title, std::move(cards[i].body), is_selected ? DOUBLE : ROUNDED)
                               | size(WIDTH, EQUAL, _options.card_width);
                if (is_selected)
                    item = this->Focused() ? (item | ftxui::focus) : (item | ftxui::select);
                items.push_back(std::move(item));
            }
            return flexbox(std::move(items)) | yframe | vscroll_indicator | reflect(_box);
        }

        bool OnEvent(Event event) override
        {
            if (_keys.empty())
                return false;
            const size_t last = _keys.size() - 1;
            const size_t columns = std::max(1, (_box.x_max - _box.x_min + 1) / std::max(1, _options.card_width));
            size_t index = this->selected_index();
            if (event == Event::ArrowLeft)
                index = index == 0 ? 0 : index - 1;
            else if (event == Event::ArrowRight)
                index = std::min(index + 1, last);
            else if (event == Event::ArrowUp)
                index = index >= columns ? index - columns : index;
            else if (event == Event::ArrowDown)
                index = std::min(index + columns, last);
            else if (event == Event::Home)
                index = 0;
            else if (event == Event::End)
                index = last;
            else
                return false;
            *_options.selected = _keys[index];
            return true;
        }

        bool Focusable() const override { return true; }

    private:
        size_t selected_index() const
        {
            const auto it = std::find(_keys.begin(), _keys.end(), *_options.selected);
            return it == _keys.end() ? 0 : static_cast<size_t>(it - _keys.begin());
        }

        CardGridOptions _options;
        std::vector<std::string> _keys;
        Box _box;
};

} // namespace

Component CardGridComponent(CardGridOptions && options)
{
    return Make<CardGrid>(std::move(options));
}

} // namespace sihd::tui

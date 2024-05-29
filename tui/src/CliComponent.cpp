#include <list>

#include <ftxui/component/captured_mouse.hpp>     // for ftxui
#include <ftxui/component/component.hpp>          // for CatchEvent, Renderer
#include <ftxui/component/event.hpp>              // for Event
#include <ftxui/component/screen_interactive.hpp> // for ScreenInteractive
#include <ftxui/dom/elements.hpp>                 // for text, vbox, window, Element, Elements

#include <sihd/tui/CliComponent.hpp>

namespace sihd::tui
{

using namespace ftxui;

namespace
{

class CliBase: public ComponentBase
{
    public:
        CliBase(std::function<void(std::string)> && on_input, CliOptions && options):
            _options(std::move(options)),
            _on_input(std::move(on_input))
        {
            auto default_options = InputOption::Default();
            default_options.content = _input_str;
            default_options.on_enter = [this]() {
                this->on_enter();
            };
            _input_cpt = Input(default_options);
        }
        ~CliBase() {}

    private:
        Element Render() final
        {
            auto focused = Focused() ? focus : ftxui::select;
            auto style = Focused() ? inverted : nothing;

            Element background = ComponentBase::Render();
            background->ComputeRequirement();
            _computed_size = background->requirement().min_y;
            return dbox({
                       std::move(background),
                       vbox({
                           text(L"") | size(HEIGHT, EQUAL, _focused_line),
                           text(L"") | style | focused,
                       }),
                   })
                   | vscroll_indicator | yframe | yflex | reflect(_log_box);
        }

        bool OnEvent(Event event) final
        {
            if (event.is_mouse() && _log_box.Contain(event.mouse().x, event.mouse().y))
                TakeFocus();

            if (event == Event::ArrowUp || (event.is_mouse() && event.mouse().button == Mouse::WheelUp))
                _focused_line--;
            else if ((event == Event::ArrowDown || (event.is_mouse() && event.mouse().button == Mouse::WheelDown)))
                _focused_line++;
            else if (event == Event::PageDown)
                _focused_line += _log_box.y_max - _log_box.y_min;
            else if (event == Event::PageUp)
                _focused_line -= _log_box.y_max - _log_box.y_min;
            else if (event == Event::Home)
                _focused_line = 0;
            else if (event == Event::End)
                _focused_line = _computed_size;
            else
                return ComponentBase::OnEvent(event);

            _focused_line = std::max((size_t)0, std::min(_computed_size, _focused_line));
            return true;
        }

        bool Focusable() const final { return true; }

        void on_enter() { _input_str = ""; }

        size_t _focused_line = 0;
        size_t _computed_size = 0;
        Box _log_box;
        CliOptions _options;
        Component _input_cpt;
        std::string _input_str;
        std::function<void(std::string)> _on_input;
};

} // namespace

ftxui::Component CliComponent(std::function<void(std::string)> && on_input, CliOptions && options)
{
    return Make<CliBase>(std::move(on_input), std::move(options));
}

} // namespace sihd::tui
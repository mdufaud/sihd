#include <algorithm>
#include <list>

#include <ftxui/component/captured_mouse.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <sihd/tui/CliComponent.hpp>

namespace sihd::tui
{

using namespace ftxui;

namespace
{

class CliBase: public ComponentBase,
               public Cli
{
    public:
        CliBase(std::function<void(Cli *, std::string_view)> && on_input, CliOptions && options):
            _options(std::move(options)),
            _on_input(std::move(on_input))
        {
            InputOption input_options;
            input_options.content = &_input_str;
            input_options.placeholder = "> ";
            input_options.cursor_position = &_cursor_pos;
            input_options.multiline = false;
            input_options.transform = [](InputState state) {
                return state.element;
            };
            input_options.on_enter = [this]() {
                this->on_enter();
            };
            input_options.on_change = [this]() {
                _tab_matches.clear();
                _tab_idx = 0;
            };
            _input_cpt = Input(input_options);
            Add(_input_cpt);
        }
        ~CliBase() = default;

        void add_output(std::string_view line) override
        {
            if (_lines.size() >= _options.max_lines)
                _lines.pop_front();
            _lines.emplace_back(line);
            if (_options.scroll_to_last_output)
                _focused_line = (ssize_t)_lines.size() - 1;
        }

    private:
        Element OnRender() override
        {
            const auto is_focused = Focused();

            Elements elements;
            elements.reserve(_lines.size());
            size_t i = 0;
            for (const auto & line : _lines)
            {
                const auto focus_management = is_focused && (_focused_line == (ssize_t)i) ? focus : nothing;
                const auto style = is_focused && (_focused_line == (ssize_t)i) ? inverted : nothing;
                elements.push_back(text(line) | focus_management | style);
                ++i;
            }

            auto output_element = vbox(elements) | vscroll_indicator | yframe | yflex | reflect(_log_box);

            auto input_line = hbox({text("> "), _input_cpt->Render()});

            if (_tab_matches.size() > 1)
            {
                Elements completion_els;
                for (size_t j = 0; j < _tab_matches.size(); ++j)
                {
                    auto dec = (j == _tab_idx) ? inverted : nothing;
                    completion_els.push_back(text(_tab_matches[j]) | dec);
                    completion_els.push_back(text("  "));
                }
                return vbox({
                    output_element,
                    separator(),
                    hflow(completion_els) | size(HEIGHT, LESS_THAN, 3),
                    input_line,
                });
            }

            return vbox({
                output_element,
                separator(),
                input_line,
            });
        }

        bool OnEvent(Event event) final
        {
            if (event == Event::Tab)
            {
                handle_tab(false);
                return true;
            }
            if (event == Event::TabReverse)
            {
                handle_tab(true);
                return true;
            }

            if (event == Event::ArrowUp)
            {
                if (navigate_history(-1))
                    return true;
            }
            if (event == Event::ArrowDown)
            {
                if (navigate_history(1))
                    return true;
            }

            if (_input_cpt->OnEvent(event))
                return true;

            if (event.is_mouse() && _log_box.Contain(event.mouse().x, event.mouse().y))
                TakeFocus();

            if (event == Event::ArrowUp || (event.is_mouse() && event.mouse().button == Mouse::WheelUp))
                _focused_line--;
            else if (event == Event::ArrowDown
                     || (event.is_mouse() && event.mouse().button == Mouse::WheelDown))
                _focused_line++;
            else if (event == Event::PageDown)
                _focused_line += _log_box.y_max - _log_box.y_min;
            else if (event == Event::PageUp)
                _focused_line -= _log_box.y_max - _log_box.y_min;
            else if (event == Event::Home)
                _focused_line = 0;
            else if (event == Event::End)
                _focused_line = (ssize_t)_lines.size() - 1;
            else
                return false;

            _focused_line = std::max((ssize_t)0, std::min((ssize_t)_lines.size() - 1, _focused_line));
            return true;
        }

        bool Focusable() const final { return ComponentBase::Focusable(); }

        void handle_tab(bool reverse)
        {
            if (_tab_matches.empty())
            {
                build_matches();
                if (_tab_matches.empty())
                    return;
                if (_tab_matches.size() == 1)
                {
                    apply_completion(_tab_matches[0]);
                    _tab_matches.clear();
                    _tab_idx = 0;
                    return;
                }
                // Multiple matches — apply first one
                apply_completion(_tab_matches[_tab_idx]);
                return;
            }

            if (reverse)
                _tab_idx = (_tab_idx == 0) ? _tab_matches.size() - 1 : _tab_idx - 1;
            else
                _tab_idx = (_tab_idx + 1) % _tab_matches.size();

            apply_completion(_tab_matches[_tab_idx]);
        }

        void build_matches()
        {
            _tab_matches.clear();
            _tab_idx = 0;

            auto last_space = _input_str.rfind(' ');
            bool completing_cmd = (last_space == std::string::npos);

            if (completing_cmd)
            {
                for (const auto & [cmd, _] : _options.completions)
                {
                    if (_input_str.empty() || cmd.starts_with(_input_str))
                        _tab_matches.push_back(cmd);
                }
            }
            else
            {
                std::string cmd_name = _input_str.substr(0, _input_str.find(' '));
                std::string partial = _input_str.substr(last_space + 1);

                auto it = _options.completions.find(cmd_name);
                if (it != _options.completions.end())
                {
                    for (const auto & arg : it->second)
                    {
                        if (partial.empty() || arg.starts_with(partial))
                            _tab_matches.push_back(arg);
                    }
                }
            }

            std::sort(_tab_matches.begin(), _tab_matches.end());
        }

        void apply_completion(const std::string & match)
        {
            auto last_space = _input_str.rfind(' ');
            if (last_space == std::string::npos)
                _input_str = match;
            else
                _input_str = _input_str.substr(0, last_space + 1) + match;
            _cursor_pos = (int)_input_str.size();
        }

        bool navigate_history(int direction)
        {
            if (_history.empty())
                return false;

            _history_idx += direction;
            _history_idx = std::max((ssize_t)-1, std::min((ssize_t)_history.size() - 1, _history_idx));

            if (_history_idx < 0)
            {
                _input_str.clear();
                _cursor_pos = 0;
                _history_idx = -1;
                return true;
            }

            auto it = _history.begin();
            std::advance(it, _history_idx);
            _input_str = *it;
            _cursor_pos = (int)_input_str.size();
            return true;
        }

        void on_enter()
        {
            _tab_matches.clear();
            _tab_idx = 0;

            if (_lines.size() >= _options.max_lines)
                _lines.pop_front();

            _lines.push_back("> " + _input_str);

            if (!_input_str.empty())
            {
                _history.push_back(_input_str);
                if (_history.size() > 100)
                    _history.pop_front();
            }
            _history_idx = -1;

            if (_on_input)
                _on_input(this, _input_str);

            _input_str.clear();
            _cursor_pos = 0;

            if (_options.scroll_to_last_output)
                _focused_line = (ssize_t)_lines.size() - 1;
        }

        ssize_t _focused_line = 0;
        Box _log_box;
        CliOptions _options;
        Component _input_cpt;
        std::string _input_str;
        int _cursor_pos = 0;
        std::list<std::string> _lines;
        std::list<std::string> _history;
        ssize_t _history_idx = -1;
        std::vector<std::string> _tab_matches;
        size_t _tab_idx = 0;
        std::function<void(Cli *, std::string_view)> _on_input;
};

} // namespace

ftxui::Component CliComponent(std::function<void(Cli *, std::string_view)> && on_input, CliOptions && options)
{
    return Make<CliBase>(std::move(on_input), std::move(options));
}

} // namespace sihd::tui
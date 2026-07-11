#include <algorithm>
#include <list>
#include <mutex>

#include <ftxui/component/captured_mouse.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <sihd/tui/CliComponent.hpp>

#include "InputLine.hpp"

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
            auto input_line = Make<InputLine>(_options.prompt, _options.placeholder);
            _input = input_line.get();
            _input->on_enter = [this] {
                this->on_enter();
            };
            // only typing triggers this: set_content from history/completion does not
            _input->on_change = [this] {
                _tab_matches.clear();
                _tab_idx = 0;
                _history_idx = -1;
                _history_prefix.clear();
            };
            _input_cpt = std::move(input_line);
            Add(_input_cpt);
        }
        ~CliBase() = default;

        // Thread-safe: may be called from any thread
        void add_output(std::string_view line, OutputStyle style) override
        {
            std::lock_guard<std::mutex> lock(_lines_mutex);
            if (_pending_lines.size() >= _options.max_lines)
                _pending_lines.pop_front();
            _pending_lines.emplace_back(OutputLine {.text = std::string(line), .style = style});
        }

        // Thread-safe: may be called from any thread
        void clear_output() override
        {
            std::lock_guard<std::mutex> lock(_lines_mutex);
            _pending_lines.clear();
            _cleared = true;
        }

    private:
        struct OutputLine
        {
                std::string text;
                OutputStyle style;
                bool is_command = false;
        };

        static Decorator style_decorator(OutputStyle style)
        {
            switch (style)
            {
                case OutputStyle::info:
                    return ftxui::color(Color::GrayLight);
                case OutputStyle::success:
                    return ftxui::color(Color::GreenLight);
                case OutputStyle::warning:
                    return ftxui::color(Color::Orange1);
                case OutputStyle::error:
                    return ftxui::color(Color::RedLight);
                case OutputStyle::normal:
                    break;
            }
            return nothing;
        }

        Element OnRender() override
        {
            // Drain pending lines produced by other threads
            {
                std::lock_guard<std::mutex> lock(_lines_mutex);
                if (_cleared)
                {
                    _lines.clear();
                    _cleared = false;
                }
                _lines.splice(_lines.end(), _pending_lines);
            }
            while (_lines.size() > _options.max_lines)
                _lines.pop_front();

            const auto is_focused = Focused();

            ssize_t focus_idx;
            if (_options.scroll_to_last_output)
                focus_idx = (ssize_t)_lines.size() - 1;
            else
            {
                _focused_line = std::clamp(_focused_line, (ssize_t)0, std::max((ssize_t)0, (ssize_t)_lines.size() - 1));
                focus_idx = _focused_line;
            }

            Elements elements;
            elements.reserve(_lines.size());
            size_t i = 0;
            for (const auto & line : _lines)
            {
                // focus drives yframe scrolling; inverted highlights only in manual-browse mode
                const auto scroll_focus = (focus_idx == (ssize_t)i) ? focus : nothing;
                const auto style = (is_focused && !_options.scroll_to_last_output && focus_idx == (ssize_t)i)
                                       ? inverted
                                       : nothing;
                const auto emphasis = line.is_command ? bold : nothing;
                elements.push_back(text(line.text) | style_decorator(line.style) | emphasis | scroll_focus | style);
                ++i;
            }

            auto output_element = vbox(elements) | vscroll_indicator | yframe | yflex | reflect(_log_box);

            auto input_element = _input_cpt->Render() | reflect(_input_box);

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
                    input_element,
                });
            }

            return vbox({
                output_element,
                separator(),
                input_element,
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
            if (event == Event::Escape && !_tab_matches.empty())
            {
                cancel_completion();
                return true;
            }
            if (event == Event::CtrlL)
            {
                this->clear_output();
                return true;
            }

            // Up/Down always consumed for history: prevents fallthrough to output scrolling
            if (event == Event::ArrowUp)
            {
                navigate_history(1);
                return true;
            }
            if (event == Event::ArrowDown)
            {
                navigate_history(-1);
                return true;
            }

            if (_input_cpt->OnEvent(event))
                return true;

            if (event.is_mouse() && _input_box.Contain(event.mouse().x, event.mouse().y))
            {
                _input_cpt->TakeFocus();
                return true;
            }

            if (event.is_mouse() && _log_box.Contain(event.mouse().x, event.mouse().y))
                _input_cpt->TakeFocus();

            if (event == Event::End)
            {
                _options.scroll_to_last_output = true;
                _focused_line = (ssize_t)_lines.size() - 1;
                return true;
            }

            // leaving auto-scroll: browsing starts from the last line, not from the top
            if (_options.scroll_to_last_output)
                _focused_line = (ssize_t)_lines.size() - 1;

            // Arrow keys are fully consumed above for history; only mouse wheel scrolls output
            if (event.is_mouse() && event.mouse().button == Mouse::WheelUp)
                _focused_line--;
            else if (event.is_mouse() && event.mouse().button == Mouse::WheelDown)
                _focused_line++;
            else if (event == Event::PageDown)
                _focused_line += _log_box.y_max - _log_box.y_min;
            else if (event == Event::PageUp)
                _focused_line -= _log_box.y_max - _log_box.y_min;
            else if (event == Event::Home)
                _focused_line = 0;
            else
                return false;

            _options.scroll_to_last_output = false;
            _focused_line = std::max((ssize_t)0, std::min((ssize_t)_lines.size() - 1, _focused_line));
            return true;
        }

        // Expose the input as the focused leaf so that ftxui routes cursor rendering
        // and focus state to it (required for Input::OnEvent to process characters).
        Component ActiveChild() override { return _input_cpt; }

        bool Focusable() const final { return ComponentBase::Focusable(); }

        void handle_tab(bool reverse)
        {
            if (_tab_matches.empty())
            {
                _tab_input_before = _input->content();
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
                // Multiple matches — complete up to their longest common prefix and show the list
                apply_completion(longest_common_prefix());
                _tab_idx = no_selection;
                return;
            }

            if (_tab_idx == no_selection)
                _tab_idx = reverse ? _tab_matches.size() - 1 : 0;
            else if (reverse)
                _tab_idx = (_tab_idx == 0) ? _tab_matches.size() - 1 : _tab_idx - 1;
            else
                _tab_idx = (_tab_idx + 1) % _tab_matches.size();

            apply_completion(_tab_matches[_tab_idx]);
        }

        std::string longest_common_prefix() const
        {
            std::string prefix = _tab_matches.front();
            for (const auto & match : _tab_matches)
            {
                size_t i = 0;
                while (i < prefix.size() && i < match.size() && prefix[i] == match[i])
                    ++i;
                prefix.resize(i);
            }
            return prefix;
        }

        void cancel_completion()
        {
            _input->set_content(_tab_input_before);
            _tab_matches.clear();
            _tab_idx = 0;
        }

        void build_matches()
        {
            _tab_matches.clear();
            _tab_idx = 0;

            const std::string & input_str = _input->content();
            auto last_space = input_str.rfind(' ');
            bool completing_cmd = (last_space == std::string::npos);

            if (completing_cmd)
            {
                for (const auto & [cmd, _] : _options.completions)
                {
                    if (input_str.empty() || cmd.starts_with(input_str))
                        _tab_matches.push_back(cmd);
                }
            }
            else
            {
                std::string cmd_name = input_str.substr(0, input_str.find(' '));
                std::string partial = input_str.substr(last_space + 1);

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
            const std::string & input_str = _input->content();
            auto last_space = input_str.rfind(' ');
            if (last_space == std::string::npos)
                _input->set_content(match);
            else
                _input->set_content(input_str.substr(0, last_space + 1) + match);
        }

        bool navigate_history(int direction)
        {
            if (_history.empty())
                return false;

            if (_history_idx < 0)
            {
                if (direction <= 0)
                    return false;
                // entering history: keep the edited line and search only entries starting with it
                _stashed_input = _input->content();
                _history_prefix = _stashed_input;
            }

            // idx 0 = most recently entered
            std::vector<const std::string *> matches;
            for (auto it = _history.rbegin(); it != _history.rend(); ++it)
            {
                if (_history_prefix.empty() || it->starts_with(_history_prefix))
                    matches.push_back(&*it);
            }
            if (matches.empty())
                return false;

            _history_idx += direction;
            _history_idx = std::max((ssize_t)-1, std::min((ssize_t)matches.size() - 1, _history_idx));

            _input->set_content(_history_idx < 0 ? _stashed_input : *matches[_history_idx]);
            return true;
        }

        void on_enter()
        {
            _tab_matches.clear();
            _tab_idx = 0;

            // copied: the input is cleared before the callback returns
            const std::string input_str = _input->content();

            // Echo directly on UI thread; trim is handled by OnRender after pending drain
            _lines.push_back(OutputLine {
                .text = _options.prompt + input_str,
                .style = OutputStyle::normal,
                .is_command = true,
            });

            // no consecutive duplicates
            if (!input_str.empty() && (_history.empty() || _history.back() != input_str))
            {
                _history.push_back(input_str);
                if (_history.size() > 100)
                    _history.pop_front();
            }
            _history_idx = -1;
            _history_prefix.clear();
            _stashed_input.clear();

            if (_on_input)
                _on_input(this, input_str);

            _input->clear();
        }

        ssize_t _focused_line = 0;
        Box _log_box;
        Box _input_box;
        CliOptions _options;
        Component _input_cpt;
        InputLine *_input = nullptr;
        bool _cleared = false;
        std::list<OutputLine> _lines;
        std::list<std::string> _history;
        ssize_t _history_idx = -1;
        std::string _history_prefix;
        std::string _stashed_input;
        static constexpr size_t no_selection = (size_t)-1;
        std::vector<std::string> _tab_matches;
        size_t _tab_idx = 0;
        std::string _tab_input_before;
        std::function<void(Cli *, std::string_view)> _on_input;
        std::mutex _lines_mutex;
        std::list<OutputLine> _pending_lines;
};

} // namespace

ftxui::Component CliComponent(std::function<void(Cli *, std::string_view)> && on_input, CliOptions && options)
{
    return Make<CliBase>(std::move(on_input), std::move(options));
}

} // namespace sihd::tui
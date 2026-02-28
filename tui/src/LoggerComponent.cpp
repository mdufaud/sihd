#include <list>
#include <mutex>

#include <ftxui/component/captured_mouse.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <sihd/tui/LoggerComponent.hpp>
#include <sihd/util/ALogger.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/LoggerManager.hpp>

namespace sihd::tui
{

using namespace ftxui;

namespace
{

class LoggerBase: public ComponentBase,
                  public sihd::util::ALogger
{
    public:
        LoggerBase(LoggerOptions && options): _options(std::move(options))
        {
            sihd::util::LoggerManager::get()->add_logger(this);
            if (_options.add_bar)
            {
                InputOption filter_opts;
                filter_opts.multiline = false;
                filter_opts.on_enter = [this] {
                    _str_filter = _str_tmp_filter;
                };
                filter_opts.transform = [](InputState state) {
                    return state.element;
                };
                _filter_input = Input(&_str_tmp_filter, "Filter", filter_opts);
                Add(_filter_input);
            }
        }
        ~LoggerBase() { sihd::util::LoggerManager::get()->remove_logger(this); }

    private:
        struct SavedLog
        {
                sihd::util::LogInfo info;
                std::string msg;
        };

        bool check_filter(const SavedLog & log)
        {
            if (_options.add_bar == false || _selected_level_filter == 0)
                return true;
            return (_selected_level_filter - 1) >= (int)log.info.level;
        }

        bool check_search(const SavedLog & log)
        {
            if (_options.add_bar == false || _str_filter.empty())
                return true;
            return log.msg.find(_str_filter) != std::string::npos
                   || log.info.source.find(_str_filter) != std::string::npos
                   || log.info.thread_name.find(_str_filter) != std::string::npos;
        }

        Element OnRender() override
        {
            // Drain pending logs from other threads
            {
                std::lock_guard<std::mutex> lock(_log_mutex);
                _saved_logs.splice(_saved_logs.end(), _pending_logs);
            }
            while (_saved_logs.size() > _options.max_logs)
            {
                bool front_visible = check_filter(_saved_logs.front()) && check_search(_saved_logs.front());
                _saved_logs.pop_front();
                if (!_options.scroll_to_last_log && front_visible)
                    _focused_log = std::max((ssize_t)0, _focused_log - 1);
            }

            const auto is_focused = Focused();

            Elements elements;
            elements.reserve(_saved_logs.size());
            for (auto & saved_log : _saved_logs)
            {
                if (check_filter(saved_log) && check_search(saved_log))
                    elements.push_back(make_log(saved_log));
            }
            _visible_count = (ssize_t)elements.size();

            ssize_t focus_idx;
            if (_options.scroll_to_last_log)
                focus_idx = _visible_count - 1;
            else
            {
                _focused_log = std::clamp(_focused_log, (ssize_t)0, std::max((ssize_t)0, _visible_count - 1));
                focus_idx = _focused_log;
            }

            if (is_focused && focus_idx >= 0 && focus_idx < _visible_count)
                elements[focus_idx] = elements[focus_idx] | focus | inverted;

            auto log_element = vbox(elements) | vscroll_indicator | yframe | yflex | reflect(_log_box);

            if (!_options.add_bar)
                return log_element;

            auto level_label = text(" " + _level_entries[_selected_level_filter] + " ▼ ") | inverted
                               | reflect(_level_btn_box);
            auto scroll_label
                = text(_options.scroll_to_last_log ? " ● scroll " : " ○ scroll ") | reflect(_scroll_btn_box);
            auto bar = hbox({
                           _filter_input->Render() | flex_grow,
                           separator(),
                           level_label,
                           separator(),
                           scroll_label,
                       })
                       | size(HEIGHT, EQUAL, 1);

            auto main_content = vbox({bar, separator(), log_element});

            if (!_show_level_menu)
                return main_content;

            Elements menu_items;
            for (size_t j = 0; j < _level_entries.size(); ++j)
            {
                auto entry_style = ((int)j == _selected_level_filter) ? inverted : nothing;
                menu_items.push_back(text(" " + _level_entries[j] + " ") | _level_colors[j] | entry_style);
            }
            auto menu_element
                = vbox(menu_items) | bgcolor(Color::Black) | border | clear_under | reflect(_menu_box);

            return dbox({
                main_content,
                vbox({
                    filler() | size(HEIGHT, EQUAL, 1),
                    hbox({filler(), menu_element, filler() | size(WIDTH, EQUAL, 12)}),
                }),
            });
        }

        bool OnEvent(Event event) final
        {
            if (_show_level_menu)
            {
                if (event.is_mouse() && event.mouse().button == Mouse::Left
                    && event.mouse().motion == Mouse::Released)
                {
                    if (_menu_box.Contain(event.mouse().x, event.mouse().y))
                    {
                        int entry = event.mouse().y - _menu_box.y_min - 1;
                        if (entry >= 0 && entry < (int)_level_entries.size())
                            _selected_level_filter = entry;
                    }
                    _show_level_menu = false;
                    return true;
                }
                if (event == Event::Escape)
                {
                    _show_level_menu = false;
                    return true;
                }
                return true;
            }

            if (event.is_mouse() && event.mouse().button == Mouse::Left
                && event.mouse().motion == Mouse::Released)
            {
                if (_options.add_bar && _level_btn_box.Contain(event.mouse().x, event.mouse().y))
                {
                    _show_level_menu = true;
                    return true;
                }
                if (_options.add_bar && _scroll_btn_box.Contain(event.mouse().x, event.mouse().y))
                {
                    _options.scroll_to_last_log = !_options.scroll_to_last_log;
                    if (!_options.scroll_to_last_log)
                        _focused_log = std::max((ssize_t)0, _visible_count - 1);
                    return true;
                }
            }

            if (_options.add_bar && _filter_input->OnEvent(event))
                return true;

            if (event.is_mouse() && _log_box.Contain(event.mouse().x, event.mouse().y))
                TakeFocus();

            if (event == Event::ArrowUp || (event.is_mouse() && event.mouse().button == Mouse::WheelUp))
                _focused_log--;
            else if (event == Event::ArrowDown
                     || (event.is_mouse() && event.mouse().button == Mouse::WheelDown))
                _focused_log++;
            else if (event == Event::PageDown)
                _focused_log += _log_box.y_max - _log_box.y_min;
            else if (event == Event::PageUp)
                _focused_log -= _log_box.y_max - _log_box.y_min;
            else if (event == Event::Home)
                _focused_log = 0;
            else if (event == Event::End)
                _focused_log = _visible_count - 1;
            else
                return ComponentBase::OnEvent(event);

            _options.scroll_to_last_log = false;
            _focused_log = std::max((ssize_t)0, std::min(_visible_count - 1, _focused_log));
            return true;
        }

        bool Focusable() const final
        {
            if (_filter_input)
                return ComponentBase::Focusable();
            return true;
        }

        Element make_log(const SavedLog & log)
        {
            const char *level;
            ftxui::Decorator color = nullptr;

            switch (log.info.level)
            {
                case sihd::util::LogLevel::emergency:
                    level = "EMER";
                    color = ftxui::color(Color::Magenta1) | bold;
                    break;
                case sihd::util::LogLevel::alert:
                    level = "A";
                    color = ftxui::color(Color::DarkViolet) | bold;
                    break;
                case sihd::util::LogLevel::critical:
                    level = "C";
                    color = ftxui::color(Color::DeepPink1) | bold;
                    break;
                case sihd::util::LogLevel::error:
                    level = "E";
                    color = ftxui::color(Color::RedLight);
                    break;
                case sihd::util::LogLevel::warning:
                    level = "W";
                    color = ftxui::color(Color::Orange1);
                    break;
                case sihd::util::LogLevel::notice:
                    level = "N";
                    color = ftxui::color(Color::GrayLight);
                    break;
                case sihd::util::LogLevel::info:
                    level = "I";
                    color = ftxui::color(Color::GreenLight);
                    break;
                case sihd::util::LogLevel::debug:
                    level = "D";
                    color = ftxui::color(Color::GrayDark);
                    break;
                default:
                    color = ftxui::color(Color::Default);
                    level = "-";
            }
            const std::string log_str = fmt::format("{} {} [{}] <{}> {}",
                                                    log.info.timestamp().local_format("%H:%M:%S"),
                                                    level,
                                                    log.info.thread_name.data(),
                                                    log.info.source.data(),
                                                    log.msg.data());

            return paragraph(log_str) | color;
        }

        void log(const sihd::util::LogInfo & info, std::string_view msg) override
        {
            std::lock_guard<std::mutex> lock(_log_mutex);
            _pending_logs.emplace_back(SavedLog {
                .info = info,
                .msg = std::string(msg),
            });
        }

        ssize_t _focused_log = 0;
        ssize_t _visible_count = 0;
        bool _show_level_menu = false;
        int _selected_level_filter = 0;

        std::string _str_tmp_filter;
        std::string _str_filter;
        std::vector<std::string> _level_entries
            = {"all", "emergency", "alert", "critical", "error", "warning", "notice", "info", "debug"};
        std::vector<Decorator> _level_colors = {
            nothing,
            ftxui::color(Color::Magenta1) | bold,
            ftxui::color(Color::DarkViolet) | bold,
            ftxui::color(Color::DeepPink1) | bold,
            ftxui::color(Color::RedLight),
            ftxui::color(Color::Orange1),
            ftxui::color(Color::GrayLight),
            ftxui::color(Color::GreenLight),
            ftxui::color(Color::GrayDark),
        };

        LoggerOptions _options;
        Box _log_box;
        Box _level_btn_box;
        Box _scroll_btn_box;
        Box _menu_box;
        Component _filter_input;
        std::mutex _log_mutex;
        std::list<SavedLog> _pending_logs;
        std::list<SavedLog> _saved_logs;
};

} // namespace

ftxui::Component LoggerComponent(LoggerOptions && options)
{
    return Make<LoggerBase>(std::move(options));
}

} // namespace sihd::tui
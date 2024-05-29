#include <list>

#include <ftxui/component/captured_mouse.hpp>     // for ftxui
#include <ftxui/component/component.hpp>          // for CatchEvent, Renderer
#include <ftxui/component/event.hpp>              // for Event
#include <ftxui/component/screen_interactive.hpp> // for ScreenInteractive
#include <ftxui/dom/elements.hpp>                 // for text, vbox, window, Element, Elements

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
            make_bar();
        }
        ~LoggerBase() { sihd::util::LoggerManager::get()->remove_logger(this); }

    private:
        struct SavedLog
        {
                sihd::util::LogInfo info;
                std::string msg;
        };

        struct BarElements
        {
                Component renderer;
                Component container;
                Component container_btn_scroll;
                Component container_search;
                Component container_level_filter;
                int selected_level_filter = 0;
                std::vector<std::string> level_entries
                    = {"all", "emergency", "alert", "critical", "error", "warning", "notice", "info", "debug"};
                std::string str_tmp_filter;
                std::string str_filter;
        };

        void make_bar()
        {
            if (_options.add_bar == false)
                return;
            _bar.container = Container::Horizontal({});
            _bar.container_btn_scroll = Container::Horizontal({});
            _bar.container_search = Container::Horizontal({});
            _bar.container_level_filter = Container::Horizontal({});

            _bar.container->Add(_bar.container_btn_scroll);
            _bar.container->Add(_bar.container_search);
            _bar.container->Add(_bar.container_level_filter);

            make_bar_btn_scroll();
            make_bar_search();
            make_bar_filter();

            _bar.renderer = Renderer(_bar.container, [this] {
                return hbox({
                    _bar.container_search->Render() | size(HEIGHT, EQUAL, 1) | flex_grow,
                    separator(),
                    _bar.container_level_filter->Render(),
                    separator(),
                    _bar.container_btn_scroll->Render(),
                });
            });

            Add(_bar.renderer);
        }

        void make_bar_search()
        {
            InputOption filter_options;
            filter_options.multiline = false;
            filter_options.on_enter = [this] {
                _bar.str_filter = _bar.str_tmp_filter;
            };
            _bar.container_search->Add(Input(&_bar.str_tmp_filter, "Filter", filter_options));
        }

        void make_bar_filter()
        {
            _bar.container_level_filter->Add(Dropdown(&_bar.level_entries, &_bar.selected_level_filter));
        }

        void make_bar_btn_scroll()
        {
            ButtonOption btn_scroll_options = ButtonOption::Border();
            btn_scroll_options.label = _options.scroll_to_last_log ? "stay" : "scroll";
            btn_scroll_options.on_click = [this] {
                _options.scroll_to_last_log = !_options.scroll_to_last_log;

                _bar.container_btn_scroll->ChildAt(0)->Detach();
                make_bar_btn_scroll();
            };
            _bar.container_btn_scroll->Add(Button(btn_scroll_options));
        }

        bool check_filter(const SavedLog & log)
        {
            return _options.add_bar
                   && (_bar.selected_level_filter == 0 || (_bar.selected_level_filter - 1) >= (int)log.info.level);
        }

        bool check_search(const SavedLog & log)
        {
            if (_options.add_bar == false || _bar.str_filter.empty())
                return true;
            return log.msg.find(_bar.str_filter) != std::string::npos
                   || log.info.source.find(_bar.str_filter) != std::string::npos
                   || log.info.thread_name.find(_bar.str_filter) != std::string::npos;
        }

        Element Render() final
        {
            const auto is_focused = Focused();

            Elements elements;
            elements.reserve(_saved_logs.size());
            auto it = _saved_logs.begin();
            size_t i = 0;
            while (i < _saved_logs.size())
            {
                SavedLog & saved_log = *it;

                if (check_filter(saved_log) && check_search(saved_log))
                {
                    const auto focus_management = is_focused && (_focused_log == i) ? focus : nothing;
                    const auto style = is_focused && (_focused_log == i) ? inverted : nothing;

                    elements.push_back(make_log(saved_log) | focus_management | style);
                }

                ++it;
                ++i;
            }

            const auto log_element = vbox(elements) | vscroll_indicator | yframe | yflex | reflect(_log_box);

            if (_options.add_bar)
            {
                // TODO replace with dbox
                return vbox({
                    _bar.renderer->Render(),
                    separator(),
                    log_element,
                });
            }

            return log_element;
        }

        bool OnEvent(Event event) final
        {
            if (event.is_mouse() && _log_box.Contain(event.mouse().x, event.mouse().y))
                TakeFocus();

            if (event == Event::ArrowUp || (event.is_mouse() && event.mouse().button == Mouse::WheelUp))
                _focused_log--;
            else if ((event == Event::ArrowDown || (event.is_mouse() && event.mouse().button == Mouse::WheelDown)))
                _focused_log++;
            else if (event == Event::PageDown)
                _focused_log += _log_box.y_max - _log_box.y_min;
            else if (event == Event::PageUp)
                _focused_log -= _log_box.y_max - _log_box.y_min;
            else if (event == Event::Home)
                _focused_log = 0;
            else if (event == Event::End)
                _focused_log = _saved_logs.size() - 1;
            else
                return ComponentBase::OnEvent(event);

            _focused_log = std::max((ssize_t)0, std::min((ssize_t)_saved_logs.size() - 1, _focused_log));
            return true;
        }

        bool Focusable() const final { return true; }

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
            const std::string log_str = fmt::format("{} {} [{}] <{}> {}\n",
                                                    log.info.timestamp().local_format("%H:%M:%S"),
                                                    level,
                                                    log.info.thread_name.data(),
                                                    log.info.source.data(),
                                                    log.msg.data());

            return paragraph(log_str) | color;
        }

        void log(const sihd::util::LogInfo & info, std::string_view msg)
        {
            if (_saved_logs.size() >= _options.max_logs)
            {
                _saved_logs.pop_front();
                _focused_log = std::max((ssize_t)0, _focused_log - 1);
            }

            _saved_logs.emplace_back(SavedLog {
                .info = info,
                .msg = std::string(msg),
            });

            if (_options.scroll_to_last_log)
                _focused_log = _saved_logs.size() - 1;
        }

        ssize_t _focused_log = 0;

        LoggerOptions _options;
        Box _log_box;
        std::list<SavedLog> _saved_logs;

        BarElements _bar;
};

} // namespace

ftxui::Component LoggerComponent(LoggerOptions && options)
{
    return Make<LoggerBase>(std::move(options));
}

} // namespace sihd::tui
#ifndef __SIHD_TUI_LOGGERWINDOW_HPP__
# define __SIHD_TUI_LOGGERWINDOW_HPP__

# include <ftxui/dom/node.hpp>

# include <sihd/util/ALogger.hpp>

namespace sihd::tui
{

class LoggerWindow: public sihd::util::ALogger
{
    public:
        LoggerWindow(size_t max_logs);
        ~LoggerWindow();

        ftxui::Element create(ftxui::Element title);

        void log(const sihd::util::LogInfo & info, std::string_view msg) override;

    protected:

    private:
        size_t _max_logs;
        std::list<std::string> _logs;
};

}

#endif
#ifndef __SIHD_CURSES_WINDOWLOGGER_HPP__
# define __SIHD_CURSES_WINDOWLOGGER_HPP__

# include <sihd/util/Node.hpp>
# include <sihd/util/ALogger.hpp>
# include <sihd/curses/Window.hpp>

namespace sihd::curses
{

class WindowLogger: public Window
{
    public:
        class Logger: public sihd::util::ALogger
        {
            public:
                Logger(Window *parent);
                ~Logger();

            private:
                void log(const sihd::util::LogInfo & info, std::string_view msg) final;

                Window *_parent_ptr;
        };

        WindowLogger(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~WindowLogger();

    protected:

    private:
        Logger *_logger_ptr;
};

}

#endif
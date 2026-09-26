#ifndef __SIHD_SYS_APP_HPP__
#define __SIHD_SYS_APP_HPP__

#include <array>
#include <functional>
#include <string>

#include <sihd/json/Json.hpp>
#include <sihd/sys/Daemon.hpp>
#include <sihd/sys/LoggerFile.hpp>
#include <sihd/sys/LoggerSystem.hpp>
#include <sihd/sys/signal.hpp>
#include <sihd/util/CliApp.hpp>

namespace sihd::sys
{

class App: public sihd::util::CliApp
{
    public:
        using Options = sihd::util::CliApp::Options;

        App(const Options & options);
        ~App() override;

        const Daemon & daemon() const { return _daemon; }

        // callback invoked for signals configured as "callback" in the conf
        void on_signal(std::function<void(int)> fn);

    protected:
        bool apply_sihd_conf(const sihd::json::Json & conf) override;
        int on_conf_loaded() override;
        void on_conf_reloaded() override;
        void install_logging() override;
        void uninstall_logging() override;
        void poll_events() override;

    private:
        enum class SigAction
        {
            none,
            stop,
            ignore,
            reload,
            callback,
        };

        void _set_default_signal_actions();
        void _set_signal_action(int sig, SigAction action);
        bool _signal_actions_from_json(const sihd::json::Json & json, SigAction action);
        void _install_signals();
        std::string _read_conf();

        std::array<SigAction, signal::max_signal> _sig_actions {};
        std::function<void(int)> _on_signal;
        std::string _conf_path;
        std::string _log_file_path;
        bool _log_file_append = true;
        bool _log_system = false;
        int _log_system_facility = LoggerSystem::default_facility;
        bool _daemon_run = false;
        Daemon _daemon;
        LoggerFile *_file_logger = nullptr;
        LoggerSystem *_system_logger = nullptr;
};

} // namespace sihd::sys

#endif

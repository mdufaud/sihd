#ifndef __SIHD_UTIL_CLIAPP_HPP__
#define __SIHD_UTIL_CLIAPP_HPP__

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <sihd/json/Json.hpp>
#include <sihd/util/CliInterpreter.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/LogInfo.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/Observable.hpp>
#include <sihd/util/StateMachine.hpp>

namespace sihd::util
{

class ALogger;

// the application runtime over the command interpreter: a boot statemachine, the json conf
// pipeline, the logging setup and the poll loops; the command line itself lives in CliInterpreter
class CliApp: public Observable<CliApp>,
              public CliInterpreter
{
    public:
        struct Options
        {
                std::string name = "";
                std::string description = "";
                std::string version = "";
                bool setup_logging = true;
                bool default_logger = true;
        };

        enum class State
        {
            none = 0,
            loading,
            configured,
            running,
            stopping,
            stopped,
            error = 255,
        };

        enum class Event
        {
            boot = 1,
            configure,
            run,
            terminate,
            halt,
            fail,
        };

        using Exit = CliInterpreter::Exit;

        CliApp(const Options & options);
        virtual ~CliApp();

        // full boot: parse + conf + logging + dispatch; returns the dispatched status
        int run(int argc, char **argv);

        // provides the json conf content; an empty string means no conf,
        // a thrown error aborts the loading
        void set_conf_loader(std::function<std::string()> loader);
        void on_reload(std::function<void()> fn);
        // replays the conf, the bindings, on_conf_reloaded and on_reload; does nothing without a
        // conf loader or with an empty conf; the command line is not replayed
        bool reload();

        // no-op when none is installed
        virtual void uninstall_logging();

        void stop();
        bool should_stop() const;
        // loops on poll_events() until stop() or the timeout is reached
        void wait_for_termination(std::optional<Duration> timeout = std::nullopt);
        // loops on poll_events() until stop() or the callable returns false; poll is the wait between rounds
        void loop(std::optional<Duration> poll = std::nullopt, std::function<bool()> step = nullptr);

        State state() const { return _statemachine.state(); }
        std::string state_str() const { return _statemachine.state_name(_statemachine.state()); }

    protected:
        // hooks of the statemachine events, invoked in the state the transition entered
        virtual void on_boot();
        virtual void on_configure();
        virtual void on_run();
        virtual void on_terminate();
        virtual void on_halt();
        virtual void on_fail();
        // the "sihd" conf section; the base reads logging.level, logging.console and
        // logging.sinks, leaves unknown keys to overriders; returning false fails the conf loading
        virtual bool apply_sihd_conf(const sihd::json::Json & conf);
        // between the conf application and the logging setup; nonzero aborts the boot
        virtual int on_conf_loaded();
        // after a conf reload was applied
        virtual void on_conf_reloaded();
        // the default logger: a color console on a terminal, the dated stream otherwise; installed
        // only when the conf names no sinks
        virtual ALogger *create_default_logger();
        // the conf "sinks" entries: the base creates nothing, the overriders create sinks by name; nullptr on a refusal
        virtual ALogger *create_logger_sink(const std::string & plugin,
                                            const std::string & type,
                                            const std::string & name,
                                            Node *parent);
        // false on a sink the app cannot create or configure
        virtual bool install_logging();
        bool apply_sink_filters(ALogger *logger) const;
        virtual void poll_events();

        Node & sinks();

        bool apply_conf(const sihd::json::Json & conf);
        // null when opted out or uninstalled
        ALogger *logger() const { return _logger; }

    private:
        static StateMachine<State, Event> default_statemachine();

        void _transition(Event evt);
        bool _apply_entry(Command *node, const std::string & key, const sihd::json::Json & value);
        static bool _check_logging_conf(const sihd::json::Json & logging);
        static bool _check_sinks_conf(const sihd::json::Json & sinks);
        bool _install_sinks();
        std::optional<LogLevel> _log_level() const;

        Options _options;
        std::function<std::string()> _conf_loader;
        std::function<void()> _on_reload;
        ALogger *_logger = nullptr;
        Node _sinks_node {"sinks"};
        sihd::json::Json _log_sinks_conf;
        StateMachine<State, Event> _statemachine;
        std::atomic<bool> _stop_requested {false};
};

} // namespace sihd::util

#endif

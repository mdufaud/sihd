#include <csignal>
#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include <sihd/sys/App.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/signal.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/LoggerManager.hpp>
#include <sihd/util/str.hpp>

namespace sihd::sys
{

using namespace sihd::util;

SIHD_LOGGER;

namespace
{

struct SigName
{
        std::string_view name;
        int sig;
};

constexpr SigName sig_names[] = {
    {"INT", SIGINT},
    {"TERM", SIGTERM},
    {"ABRT", SIGABRT},
#if defined(SIGALRM)
    {"ALRM", SIGALRM},
#endif
#if defined(SIGHUP)
    {"HUP", SIGHUP},
#endif
#if defined(SIGQUIT)
    {"QUIT", SIGQUIT},
#endif
#if defined(SIGUSR1)
    {"USR1", SIGUSR1},
#endif
#if defined(SIGUSR2)
    {"USR2", SIGUSR2},
#endif
#if defined(SIGPIPE)
    {"PIPE", SIGPIPE},
#endif
};

std::optional<int> sig_from_conf_value(const std::string & name)
{
    std::string upper = name;
    str::to_upper(upper);
    std::string_view stripped = upper;
    if (stripped.starts_with("SIG"))
        stripped.remove_prefix(3);
    for (const SigName & entry : sig_names)
    {
        if (entry.name == stripped)
            return entry.sig;
    }
    return std::nullopt;
}

} // namespace

App::App(const Options & options): CliApp(options), _daemon("daemon")
{
    this->_set_default_signal_actions();
    this->root().bind("conf", _conf_path, "Configuration file path");
    this->set_conf_loader([this]() { return this->_read_conf(); });
}

App::~App()
{
    for (int sig = 1; sig < signal::max_signal; ++sig)
    {
        if (_sig_actions[sig] != SigAction::none)
            signal::unhandle(sig);
    }
    this->uninstall_logging();
}

void App::on_signal(std::function<void(int)> fn)
{
    _on_signal = std::move(fn);
}

bool App::apply_sihd_conf(const sihd::json::Json & conf)
{
    // each conf application recomputes the whole signal policy from the defaults
    this->_set_default_signal_actions();
    if (CliApp::apply_sihd_conf(conf) == false)
        return false;
    if (conf.is_object() == false)
        return true;
    if (conf.contains("logging"))
    {
        const sihd::json::Json logging = conf["logging"];
        if (logging.is_object())
        {
            if (logging.contains("file"))
            {
                const sihd::json::Json file = logging["file"];
                if (file.is_object())
                {
                    _log_file_path = file["path"].get_or<std::string>("");
                    _log_file_append = file["append"].get_or<bool>(true);
                }
                else
                {
                    _log_file_path = file.get_or<std::string>("");
                }
            }
            if (logging.contains("system"))
            {
                const sihd::json::Json system = logging["system"];
                if (system.is_bool())
                {
                    _log_system = system.get<bool>();
                }
                else
                {
                    _log_system = true;
                    _log_system_facility = system["facility"].get_or<int>(LoggerSystem::default_facility);
                }
            }
        }
    }
    if (conf.contains("signals"))
    {
        const sihd::json::Json signals = conf["signals"];
        if (signals.is_object())
        {
            constexpr std::pair<std::string_view, SigAction> conf_actions[] = {
                {"stop", SigAction::stop},
                {"ignore", SigAction::ignore},
                {"reload", SigAction::reload},
                {"callback", SigAction::callback},
            };
            for (const auto & [name, action] : conf_actions)
            {
                if (signals.contains(name) && this->_signal_actions_from_json(signals[name], action) == false)
                    return false;
            }
        }
    }
    if (conf.contains("daemon"))
    {
        const sihd::json::Json daemon = conf["daemon"];
        if (daemon.is_object())
        {
            if (daemon.contains("user"))
                _daemon.set_user(daemon["user"].get_or<std::string>(""));
            if (daemon.contains("group"))
                _daemon.set_group(daemon["group"].get_or<std::string>(""));
            if (daemon.contains("pid_file"))
                _daemon.set_pid_file_path(daemon["pid_file"].get_or<std::string>(""));
            if (daemon.contains("working_dir"))
                _daemon.set_working_dir_path(daemon["working_dir"].get_or<std::string>(""));
            // daemonizing is explicit: the other keys only configure
            if (daemon.contains("run"))
                _daemon_run = daemon["run"].get_or<bool>(false);
        }
    }
    return true;
}

int App::on_conf_loaded()
{
    if (_daemon_run)
    {
        if constexpr (Daemon::supported)
        {
            // never returns in the parent process
            if (_daemon.run() == false)
                return EXIT_FAILURE;
        }
        else
        {
            SIHD_LOG(warning, "App: daemonization is not supported on this platform");
        }
    }
    this->_install_signals();
    return 0;
}

void App::on_conf_reloaded()
{
    // the reload may add, remove or change signal actions: handlers must follow
    this->_install_signals();
}

void App::install_logging()
{
    // loggers are replaced so that a reload applies logging changes
    this->uninstall_logging();
    if (_log_file_path.empty() == false)
    {
        _file_logger = new LoggerFile(_log_file_path, _log_file_append);
        this->apply_log_level(_file_logger);
        LoggerManager::add(_file_logger);
    }
    if (_log_system)
    {
        _system_logger = new LoggerSystem(this->name(), _log_system_facility);
        this->apply_log_level(_system_logger);
        LoggerManager::add(_system_logger);
    }
    CliApp::install_logging();
}

void App::uninstall_logging()
{
    if (_file_logger)
    {
        LoggerManager::rm(_file_logger);
        delete _file_logger;
        _file_logger = nullptr;
    }
    if (_system_logger)
    {
        LoggerManager::rm(_system_logger);
        delete _system_logger;
        _system_logger = nullptr;
    }
    CliApp::uninstall_logging();
}

void App::poll_events()
{
    // copied under the lock: a reload from another thread may rework the signal policy
    std::array<SigAction, signal::max_signal> actions;
    {
        std::lock_guard<std::recursive_mutex> lock(this->mutex());
        actions = _sig_actions;
    }
    for (int sig = 1; sig < signal::max_signal; ++sig)
    {
        const SigAction action = actions[sig];
        if (action == SigAction::none)
            continue;
        std::optional<signal::SigStatus> status = signal::status(sig);
        if (status.has_value() == false || status->received.load() == 0)
            continue;
        signal::reset_received(sig);
        switch (action)
        {
            case SigAction::stop:
                SIHD_LOG(info, "App: {} received, stopping", signal::name(sig));
                this->stop();
                break;
            case SigAction::none:
                break;
            case SigAction::ignore:
                break;
            case SigAction::reload:
                this->reload();
                break;
            case SigAction::callback:
                if (_on_signal)
                    _on_signal(sig);
                break;
        }
    }
}

void App::_set_default_signal_actions()
{
    _sig_actions.fill(SigAction::none);
    _sig_actions[SIGINT] = SigAction::stop;
    _sig_actions[SIGTERM] = SigAction::stop;
}

void App::_set_signal_action(int sig, SigAction action)
{
    if (sig < 1 || sig >= signal::max_signal)
    {
        SIHD_LOG(error, "App: signal {} out of range", sig);
        return;
    }
    _sig_actions[sig] = action;
}

bool App::_signal_actions_from_json(const sihd::json::Json & json, SigAction action)
{
    auto apply_one = [this, action](const sihd::json::Json & entry) -> bool {
        if (entry.is_number_integer() || entry.is_number_unsigned())
        {
            this->_set_signal_action(static_cast<int>(entry.get<int64_t>()), action);
            return true;
        }
        std::optional<int> sig = sig_from_conf_value(entry.get_or<std::string>(""));
        if (sig.has_value() == false)
        {
            SIHD_LOG(error, "App: unknown signal in conf '{}'", entry.get_or<std::string>(""));
            return false;
        }
        this->_set_signal_action(*sig, action);
        return true;
    };
    if (json.is_array())
    {
        for (auto it = json.begin(); it != json.end(); ++it)
        {
            if (apply_one(it.value()) == false)
                return false;
        }
        return true;
    }
    return apply_one(json);
}

void App::_install_signals()
{
    for (int sig = 1; sig < signal::max_signal; ++sig)
    {
        if (_sig_actions[sig] == SigAction::none)
            continue;
        bool done = _sig_actions[sig] == SigAction::ignore ? signal::ignore(sig) : signal::handle(sig);
        if (done == false)
            SIHD_LOG(error, "App: cannot set action for signal {}", sig);
    }
}

std::string App::_read_conf()
{
    if (_conf_path.empty())
        return "";
    std::optional<std::string> content = fs::read_all(_conf_path);
    if (content.has_value() == false)
        throw std::runtime_error("cannot read conf file '" + _conf_path + "'");
    return *content;
}

} // namespace sihd::sys

#include <algorithm>
#include <cstdlib>
#include <expected>
#include <iterator>

#include <sihd/util/CliApp.hpp>
#include <sihd/util/LogInfo.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/LoggerConsole.hpp>
#include <sihd/util/LoggerFilter.hpp>
#include <sihd/util/LoggerManager.hpp>
#include <sihd/util/LoggerStream.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/term.hpp>
#include <sihd/util/time.hpp>

namespace sihd::util
{

SIHD_LOGGER;

namespace
{

constexpr int poll_step_ms = 50;

constexpr std::string_view sink_entry_keys[] = {"logger",
                                                "plugin",
                                                "name",
                                                "source_regex",
                                                "thread_regex",
                                                "message_regex",
                                                "level_lower",
                                                "level_higher",
                                                "level_eq"};

constexpr uint64_t key(CliApp::State state, CliApp::Event evt)
{
    return StateMachine<CliApp::State, CliApp::Event>::pack_key(state, evt);
}

// level_from_str yields none for unknown names
LogLevel conf_level(std::string_view name)
{
    std::string upper(name);
    str::to_upper(upper);
    return LogInfo::level_from_str(upper);
}

std::optional<LogLevel> level_from_conf(const sihd::json::Json & entry, std::string_view key)
{
    if (entry.contains(key) == false)
        return std::nullopt;
    return conf_level(entry[key].get_or<std::string>(""));
}

} // namespace

StateMachine<CliApp::State, CliApp::Event> CliApp::default_statemachine()
{
    StateMachine<State, Event> machine(State::none);
    machine.set_transitions_map({
        {key(State::none, Event::boot), State::loading},
        {key(State::loading, Event::configure), State::configured},
        {key(State::configured, Event::run), State::running},
        {key(State::running, Event::terminate), State::stopping},
        {key(State::loading, Event::terminate), State::stopping},
        {key(State::stopping, Event::halt), State::stopped},
        {key(State::loading, Event::halt), State::stopped},
        {key(State::loading, Event::fail), State::error},
        {key(State::configured, Event::fail), State::error},
        {key(State::running, Event::fail), State::error},
    });
    machine.set_states_names_map({
        {State::none, "none"},
        {State::loading, "loading"},
        {State::configured, "configured"},
        {State::running, "running"},
        {State::stopping, "stopping"},
        {State::stopped, "stopped"},
        {State::error, "error"},
    });
    machine.set_events_names_map({
        {Event::boot, "boot"},
        {Event::configure, "configure"},
        {Event::run, "run"},
        {Event::terminate, "terminate"},
        {Event::halt, "halt"},
        {Event::fail, "fail"},
    });
    return machine;
}

CliApp::CliApp(const Options & options):
    CliInterpreter(options.name, options.description, options.version),
    _options(options),
    _statemachine(default_statemachine())
{
}

CliApp::~CliApp()
{
    this->uninstall_logging();
}

int CliApp::run(int argc, char **argv)
{
    std::lock_guard<std::recursive_mutex> lock(this->mutex());
    if (_statemachine.state() != State::none)
    {
        SIHD_LOG(error, "CliApp: run() called in state '{}'", _statemachine.state_name(_statemachine.state()));
        return EXIT_FAILURE;
    }
    std::vector<std::string> args(argv + 1, argv + argc); // argv[0] is the program name
    this->_transition(Event::boot);
    int help_status = this->_intercept_help(args);
    if (help_status >= 0)
    {
        this->_transition(Event::halt);
        return help_status;
    }
    int completion_status = this->_intercept_completion(args);
    if (completion_status >= 0)
    {
        this->_transition(Event::halt);
        return completion_status;
    }
    int status = this->_parse(args);
    if (status != 0)
    {
        this->_transition(Event::fail);
        return status;
    }
    if (_parse_handled)
    {
        this->_transition(Event::halt);
        return status;
    }
    // precedence: struct (current values) -> json -> cli -> --set
    this->root().snapshot_cli_bindings(false);
    if (_conf_loader)
    {
        std::string content;
        try
        {
            content = _conf_loader();
        }
        catch (const std::exception & e)
        {
            SIHD_LOG(error, "CliApp: conf loading failed: {}", e.what());
            this->_transition(Event::fail);
            return EXIT_FAILURE;
        }
        if (content.empty() == false)
        {
            auto parsed = sihd::json::Json::parse(content);
            if (!parsed)
            {
                SIHD_LOG(error, "CliApp: conf loading failed: {}", parsed.error());
                this->_transition(Event::fail);
                return EXIT_FAILURE;
            }
            if (this->apply_conf(*parsed) == false)
            {
                SIHD_LOG(error, "CliApp: conf loading failed");
                this->_transition(Event::fail);
                return EXIT_FAILURE;
            }
        }
    }
    this->root().snapshot_cli_bindings(true);
    if (this->_apply_set_overrides() == false)
    {
        this->_transition(Event::fail);
        return EXIT_FAILURE;
    }
    if (this->on_conf_loaded() != 0)
    {
        this->_transition(Event::fail);
        return EXIT_FAILURE;
    }
    if (_options.setup_logging && this->install_logging() == false)
    {
        this->_transition(Event::fail);
        return EXIT_FAILURE;
    }
    this->_transition(Event::configure);
    this->_transition(Event::run);
    status = this->_dispatch();
    this->_transition(Event::terminate);
    this->_transition(Event::halt);
    return status;
}

void CliApp::set_conf_loader(std::function<std::string()> loader)
{
    _conf_loader = std::move(loader);
}

void CliApp::on_reload(std::function<void()> fn)
{
    _on_reload = std::move(fn);
}

bool CliApp::reload()
{
    std::lock_guard<std::recursive_mutex> lock(this->mutex());
    if (!_conf_loader)
        return true;
    std::string content;
    try
    {
        content = _conf_loader();
    }
    catch (const std::exception & e)
    {
        SIHD_LOG(error, "CliApp: conf reload failed: {}", e.what());
        return false;
    }
    if (content.empty())
        return true;
    auto parsed = sihd::json::Json::parse(content);
    if (!parsed)
    {
        SIHD_LOG(error, "CliApp: conf reload failed: {}", parsed.error());
        return false;
    }
    if (this->apply_conf(*parsed) == false)
    {
        SIHD_LOG(error, "CliApp: conf reload failed");
        return false;
    }
    // command line given values still win over the replayed conf, --set stays the strongest
    this->root().snapshot_cli_bindings(true);
    if (this->_apply_set_overrides() == false)
        return false;
    if (_options.setup_logging && this->install_logging() == false)
        return false;
    this->on_conf_reloaded();
    if (_on_reload)
        _on_reload();
    return true;
}

void CliApp::stop()
{
    // only the request: run() drives the lifecycle, on its own thread
    _stop_requested = true;
}

bool CliApp::should_stop() const
{
    return _stop_requested.load();
}

void CliApp::wait_for_termination(std::optional<Duration> timeout)
{
    Duration waited(0);
    while (this->should_stop() == false)
    {
        this->poll_events();
        if (timeout.has_value() && waited >= *timeout)
            break;
        time::msleep(poll_step_ms);
        waited += Duration(time::milli(poll_step_ms));
    }
}

void CliApp::loop(std::optional<Duration> poll, std::function<bool()> step)
{
    while (this->should_stop() == false && (step == nullptr || step()))
    {
        this->poll_events();
        time::msleep(poll.value_or(Duration(time::milli(poll_step_ms))).milliseconds());
    }
}

void CliApp::on_boot() {}

void CliApp::on_configure() {}

void CliApp::on_run() {}

void CliApp::on_terminate() {}

void CliApp::on_halt() {}

void CliApp::on_fail() {}

void CliApp::_transition(Event evt)
{
    if (_statemachine.transition(evt) == false)
    {
        SIHD_LOG(debug,
                 "CliApp: event '{}' invalid in state '{}'",
                 _statemachine.event_name(evt),
                 _statemachine.state_name(_statemachine.state()));
        return;
    }
    switch (evt)
    {
        case Event::boot:
            this->on_boot();
            break;
        case Event::configure:
            this->on_configure();
            break;
        case Event::run:
            this->on_run();
            break;
        case Event::terminate:
            this->on_terminate();
            break;
        case Event::halt:
            this->on_halt();
            break;
        case Event::fail:
            this->on_fail();
            break;
    }
    this->notify_observers(this);
}

bool CliApp::_check_logging_conf(const sihd::json::Json & logging)
{
    static constexpr std::string_view logging_keys[] = {"console", "level", "sinks"};
    if (logging.is_object() == false)
    {
        SIHD_LOG(error, "CliApp: conf 'logging' must be an object");
        return false;
    }
    for (auto it = logging.begin(); it != logging.end(); ++it)
    {
        const std::string_view conf_key = it.key();
        if (std::find(std::begin(logging_keys), std::end(logging_keys), conf_key) == std::end(logging_keys))
        {
            SIHD_LOG(error, "CliApp: unknown conf key 'logging.{}'", conf_key);
            return false;
        }
        if (conf_key == "console" && logging["console"].is_bool() == false)
        {
            SIHD_LOG(error, "CliApp: conf 'logging.console' must be a boolean");
            return false;
        }
        if (conf_key == "level")
        {
            const sihd::json::Json & level = logging["level"];
            if (level.is_string() == false || conf_level(level.get_or<std::string>("")) == LogLevel::none)
            {
                SIHD_LOG(error, "CliApp: unknown log level '{}'", level.dump());
                return false;
            }
        }
        if (conf_key == "sinks" && _check_sinks_conf(logging["sinks"]) == false)
            return false;
    }
    return true;
}

bool CliApp::_check_sinks_conf(const sihd::json::Json & sinks)
{
    if (sinks.is_array() == false)
    {
        SIHD_LOG(error, "CliApp: conf 'logging.sinks' must be an array");
        return false;
    }
    for (auto it = sinks.begin(); it != sinks.end(); ++it)
    {
        const sihd::json::Json & entry = *it;
        if (entry.is_object() == false)
        {
            SIHD_LOG(error, "CliApp: conf 'logging.sinks' entries must be objects");
            return false;
        }
        if (entry.contains("logger") == false || entry["logger"].is_string() == false)
        {
            SIHD_LOG(error, "CliApp: conf 'logging.sinks' entries must name a string 'logger'");
            return false;
        }
        if (entry.contains("plugin") && entry["plugin"].is_string() == false)
        {
            SIHD_LOG(error, "CliApp: conf 'logging.sinks.plugin' must be a string");
            return false;
        }
        if (entry.contains("name") == false || entry["name"].is_string() == false)
        {
            SIHD_LOG(error, "CliApp: conf 'logging.sinks' entries must carry a string 'name'");
            return false;
        }
        // the sink-specific keys are validated at install time by the sink itself
        for (auto entry_it = entry.begin(); entry_it != entry.end(); ++entry_it)
        {
            const std::string_view entry_key = entry_it.key();
            if (std::find(std::begin(sink_entry_keys), std::end(sink_entry_keys), entry_key)
                == std::end(sink_entry_keys))
                continue;
            if (entry_it.value().is_string() == false)
            {
                SIHD_LOG(error, "CliApp: conf 'logging.sinks.{}' must be a string", entry_key);
                return false;
            }
            if (entry_key.starts_with("level_")
                && conf_level(entry_it.value().get_or<std::string>("")) == LogLevel::none)
            {
                SIHD_LOG(error, "CliApp: unknown log level '{}'", entry_it.value().dump());
                return false;
            }
        }
    }
    return true;
}

bool CliApp::apply_sihd_conf(const sihd::json::Json & conf)
{
    if (conf.is_object() == false || conf.contains("logging") == false)
        return true;
    const sihd::json::Json logging = conf["logging"];
    if (_check_logging_conf(logging) == false)
        return false;
    if (logging.contains("console"))
        _options.default_logger = logging["console"].get_or<bool>(_options.default_logger);
    if (logging.contains("level"))
        this->set_log_level_conf(logging["level"].get_or<std::string>(this->log_level_conf()));
    if (logging.contains("sinks"))
        _log_sinks_conf = logging["sinks"];
    return true;
}

int CliApp::on_conf_loaded()
{
    return 0;
}

void CliApp::on_conf_reloaded() {}

Node & CliApp::sinks()
{
    return _sinks_node;
}

ALogger *CliApp::create_default_logger()
{
    // human format with colors on a terminal, the dated format for captured output
    if (term::supports_color(stderr))
        return new LoggerConsole("console");
    return new LoggerStream(stderr);
}

ALogger *CliApp::create_logger_sink(const std::string &, const std::string &, const std::string &, Node *)
{
    return nullptr;
}

bool CliApp::apply_sink_filters(ALogger *logger) const
{
    if (_log_sinks_conf.is_array() == false)
        return true;
    for (const sihd::json::Json & entry : _log_sinks_conf)
    {
        // an entry's filters stay on the sink it names
        const std::string entry_name = entry["name"].get_or<std::string>("");
        if (entry_name != logger->name())
            continue;
        LoggerFilter::Options opts;
        opts.source_regex = entry["source_regex"].get_or<std::string>("");
        opts.thread_regex = entry["thread_regex"].get_or<std::string>("");
        opts.message_regex = entry["message_regex"].get_or<std::string>("");
        const std::pair<std::string_view, LogLevel *> level_keys[] = {
            {"level_lower", &opts.level_lower},
            {"level_higher", &opts.level_higher},
            {"level_eq", &opts.level_eq},
        };
        for (const auto & [level_key, target] : level_keys)
        {
            if (entry.contains(level_key) == false)
                continue;
            const std::optional<LogLevel> level = level_from_conf(entry, level_key);
            if (level.has_value() == false)
            {
                SIHD_LOG(error, "CliApp: unknown log level '{}'", entry[level_key].dump());
                return false;
            }
            *target = *level;
        }
        logger->add_filter(new LoggerFilter(opts));
    }
    return true;
}

bool CliApp::_install_sinks()
{
    if (_log_sinks_conf.is_array() == false)
        return true;
    for (const sihd::json::Json & entry : _log_sinks_conf)
    {
        const std::string type = entry["logger"].get_or<std::string>("");
        const std::string plugin = entry["plugin"].get_or<std::string>("");
        const std::string name = entry["name"].get_or<std::string>("");
        if (type.empty())
        {
            SIHD_LOG(error, "CliApp: sink entry must name a 'logger'");
            return false;
        }
        if (this->sinks().get_child(name) != nullptr)
        {
            SIHD_LOG(error, "CliApp: duplicate sink name '{}'", name);
            return false;
        }
        ALogger *sink = nullptr;
        try
        {
            sink = this->create_logger_sink(plugin, type, name, &this->sinks());
        }
        catch (const std::exception & e)
        {
            SIHD_LOG(error, "CliApp: cannot install logger sink '{}': {}", type, e.what());
            return false;
        }
        if (sink == nullptr)
        {
            SIHD_LOG(error, "CliApp: cannot install logger sink '{}'", type);
            return false;
        }
        for (auto it = entry.begin(); it != entry.end(); ++it)
        {
            const std::string_view sink_key = it.key();
            if (std::find(std::begin(sink_entry_keys), std::end(sink_entry_keys), sink_key)
                != std::end(sink_entry_keys))
                continue;
            std::expected<void, Error> res = sink->set_conf(std::string(sink_key), it.value());
            if (!res)
            {
                SIHD_LOG(error, "CliApp: sink '{}': {}", name, res.error().message);
                (void)this->sinks().remove_child(sink);
                return false;
            }
        }
        if (this->apply_sink_filters(sink) == false)
        {
            (void)this->sinks().remove_child(sink);
            return false;
        }
        LoggerManager::add(sink);
    }
    return true;
}

bool CliApp::install_logging()
{
    // base scope: a derived uninstall_logging would remove the loggers it installed itself
    this->CliApp::uninstall_logging();
    // an install always stamps it, absent config resets to default
    LoggerManager::set_level(this->_log_level().value_or(LogLevel::debug));
    // the sinks conf replaces the default logger entirely
    const bool has_sinks_conf = _log_sinks_conf.is_array() && _log_sinks_conf.empty() == false;
    if (_options.default_logger && has_sinks_conf == false)
    {
        _logger = this->create_default_logger();
        if (_logger != nullptr)
            LoggerManager::add(_logger);
    }
    if (this->_install_sinks() == false)
    {
        this->CliApp::uninstall_logging();
        return false;
    }
    if (this->_log_level().has_value() == false && this->log_level_conf().empty() == false)
        SIHD_LOG(warning, "CliApp: unknown log level '{}'", this->log_level_conf());
    return true;
}

void CliApp::uninstall_logging()
{
    for (const std::string & child_name : this->sinks().children_keys())
    {
        if (ALogger *sink = this->sinks().get_child<ALogger>(child_name))
            LoggerManager::rm(sink);
    }
    this->sinks().remove_children();
    if (_logger == nullptr)
        return;
    LoggerManager::rm(_logger);
    delete _logger;
    _logger = nullptr;
}

void CliApp::poll_events() {}

bool CliApp::apply_conf(const sihd::json::Json & conf)
{
    if (conf.is_object() == false)
    {
        SIHD_LOG(error, "CliApp: conf is not a json object");
        return false;
    }
    for (auto it = conf.begin(); it != conf.end(); ++it)
    {
        if (it.key() == "sihd")
        {
            if (this->apply_sihd_conf(it.value()) == false)
                return false;
        }
        else if (this->_apply_entry(&this->root(), it.key(), it.value()) == false)
            return false;
    }
    return true;
}

bool CliApp::_apply_entry(Command *node, const std::string & key, const sihd::json::Json & value)
{
    if (Command::Binding *binding = node->find_binding(key))
    {
        if (binding->apply_json(value))
            return true;
        SIHD_LOG(error, "CliApp: conf value '{}' does not fit key '{}'", value.dump(), key);
        return false;
    }
    if (Command *child = node->get_command(key))
    {
        if (value.is_object() == false)
        {
            SIHD_LOG(error, "CliApp: conf '{}' must be an object (command section)", key);
            return false;
        }
        for (auto it = value.begin(); it != value.end(); ++it)
        {
            if (this->_apply_entry(child, it.key(), it.value()) == false)
                return false;
        }
        return true;
    }
    SIHD_LOG(error, "CliApp: unknown conf key '{}'", key);
    return false;
}

std::optional<LogLevel> CliApp::_log_level() const
{
    if (this->log_level_conf().empty())
        return std::nullopt;
    const LogLevel level = conf_level(this->log_level_conf());
    if (level == LogLevel::none)
        return std::nullopt;
    return level;
}

} // namespace sihd::util

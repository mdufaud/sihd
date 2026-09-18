#include <cctype>
#include <cstdlib>

#include <fmt/format.h>

#include <sihd/util/CliInterpreter.hpp>
#include <sihd/util/LogInfo.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Splitter.hpp>
#include <sihd/util/str.hpp>

#include <CLI/CLI.hpp>

namespace sihd::util
{

SIHD_LOGGER;

namespace
{

constexpr std::string_view generate_completion_flag = "--generate-bash-completion";
constexpr std::string_view complete_command = "__complete";

// bash function names cannot carry these characters
std::string bash_identifier(std::string_view name)
{
    std::string id(name);
    for (char & c : id)
    {
        if (std::isalnum(static_cast<unsigned char>(c)) == 0 && c != '_')
            c = '_';
    }
    return id;
}

// double quotes protecting a case pattern, escaping the bash expansions
std::string bash_quote(std::string_view str)
{
    std::string out;
    out.reserve(str.size() + 2);
    out += '"';
    for (char c : str)
    {
        if (c == '"' || c == '\\' || c == '$' || c == '`')
            out += '\\';
        out += c;
    }
    out += '"';
    return out;
}

// the known values of an option, empty when they cannot be guessed
std::vector<std::string> completion_values(std::string_view option)
{
    if (option != "--log-level")
        return {};
    std::vector<std::string> names;
    names.reserve(static_cast<size_t>(static_cast<int>(LogLevel::debug)) + 1);
    for (int level = 0; level <= static_cast<int>(LogLevel::debug); ++level)
    {
        std::string name = LogInfo::level_str(static_cast<LogLevel>(level));
        str::to_lower(name);
        names.push_back(std::move(name));
    }
    return names;
}

// the case line of the static script listing what can be typed at one command path
std::string completion_node_line(std::string_view path,
                                 const std::vector<std::string> & commands,
                                 const std::vector<std::string> & options,
                                 const std::vector<std::string> & flags,
                                 bool positional)
{
    std::string key = path.empty() ? "\"\"" : bash_quote(path);
    return fmt::format("        {}) commands=\"{}\"; options=\"{}\"; flags=\"{}\"; positional={} ;;",
                       key,
                       str::join(commands, " "),
                       str::join(options, " "),
                       str::join(flags, " "),
                       positional ? 1 : 0);
}

constexpr std::string_view static_completion_template = R"bash(# bash completion for {name}
__{func}_node() {
    case "$1" in
{node_lines}
        *) commands=""; options=""; flags=""; positional=0 ;;
    esac
}
__{func}_values() {
    case "$1" in
        {log_level}) echo "{log_level_values}" ;;
    esac
}
_{func}() {
    local cur word i path pending dashed values opt positional
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    path=""
    pending=0
    dashed=0
    positional=0
    __{func}_node ""
    for ((i = 1; i < COMP_CWORD; i++)); do
        word="${COMP_WORDS[i]}"
        if [[ $pending -eq 1 ]]; then
            pending=0
            continue
        fi
        if [[ $dashed -eq 1 ]]; then
            continue
        fi
        if [[ "$word" == "--" ]]; then
            dashed=1
            continue
        fi
        if [[ "$word" == -* ]]; then
            if [[ "$word" != *=* ]]; then
                case " $options " in *" $word "*) pending=1 ;; esac
            fi
            continue
        fi
        if [[ " $commands " == *" $word "* ]]; then
            path="${path:+$path }$word"
            __{func}_node "$path"
        elif [[ $positional -eq 1 ]]; then
            : # a value filling the positional: the node keeps its candidates
        else
            commands=""; options=""; flags=""; positional=0
        fi
    done
    if [[ $dashed -eq 1 ]]; then
        return 0
    fi
    if [[ $pending -eq 1 ]]; then
        values="$(__{func}_values "${COMP_WORDS[COMP_CWORD-1]}")"
        if [[ -n "$values" ]]; then
            COMPREPLY=($(compgen -W "$values" -- "$cur"))
        fi
        return 0
    fi
    if [[ "$cur" == -* ]]; then
        if [[ "$cur" == *=* ]]; then
            opt="${cur%%=*}"
            values="$(__{func}_values "$opt")"
            COMPREPLY=($(compgen -P "$opt=" -W "$values" -- "${cur#*=}"))
        else
            COMPREPLY=($(compgen -W "$options $flags" -- "$cur"))
        fi
        return 0
    fi
    COMPREPLY=($(compgen -W "$commands" -- "$cur"))
    return 0
}
complete -F _{func} -o default {name}
)bash";

std::string
    render_static_completion(std::string_view name, std::string_view func, const std::vector<std::string> & node_lines)
{
    return str::replace(static_completion_template,
                        {{"{name}", name},
                         {"{func}", func},
                         {"{log_level}", bash_quote("--log-level")},
                         {"{log_level_values}", str::join(completion_values("--log-level"), " ")},
                         {"{node_lines}", str::join(node_lines, "\n")}});
}

constexpr std::string_view loader_completion_template = R"bash(# bash completion for {name}
_{func}() {
    local cur reply
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    reply=$( "${COMP_WORDS[0]}" {complete_command} "${COMP_WORDS[@]:1:COMP_CWORD-1}" "$cur" 2>/dev/null )
    local IFS=$'\n'
    COMPREPLY=($(compgen -W "$reply" -- "$cur"))
    return 0
}
complete -F _{func} -o default {name}
)bash";

std::string render_loader_completion(std::string_view name, std::string_view func)
{
    return str::replace(loader_completion_template,
                        {{"{name}", name}, {"{func}", func}, {"{complete_command}", complete_command}});
}

} // namespace

CliInterpreter::CliInterpreter(std::string_view name, std::string_view description, std::string_view version):
    _name(name),
    _version(version),
    _cli(std::make_unique<CLI::App>(std::string(description), _name)),
    _root(new Command(_cli.get()))
{
    // one subcommand per level
    _cli->require_subcommand(0, 1);
    _root->bind("log-level", _log_level_str, "Logging level: emergency..debug");
    _cli->add_option("--set", _set_args, "Override a conf value: --set command.path.key=value");
    if (_version.empty() == false)
        _cli->set_version_flag("--version", _version);
}

CliInterpreter::~CliInterpreter() = default;

int CliInterpreter::evaluate(const std::vector<std::string> & args)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    return this->_evaluate(args);
}

int CliInterpreter::evaluate(std::string_view line)
{
    Splitter splitter;
    splitter.set_delimiter_spaces();
    splitter.set_escape_sequences_all();
    std::vector<std::string> args;
    for (std::string_view view : splitter.split_view(line))
    {
        std::string token(str::unquote(view));
        args.push_back(str::remove_escape_char(token));
    }
    return this->evaluate(args);
}

void CliInterpreter::exit(int status)
{
    throw Exit(status);
}

std::string CliInterpreter::bash_completion() const
{
    return this->_bash_completion(false);
}

std::string CliInterpreter::bash_completion_loader() const
{
    return this->_bash_completion(true);
}

int CliInterpreter::_parse(const std::vector<std::string> & args)
{
    _parse_handled = false;
    _set_args.clear();
    try
    {
        // the argc/argv overload is the only one consuming tokens in order; a program name is prepended
        std::vector<const char *> cargv;
        cargv.reserve(args.size() + 1);
        cargv.push_back("prog");
        for (const std::string & arg : args)
            cargv.push_back(arg.c_str());
        _cli->clear();
        _cli->parse((int)cargv.size(), cargv.data());
        return 0;
    }
    catch (const CLI::ParseError & e)
    {
        int status = _cli->exit(e);
        _parse_handled = status == 0;
        return status;
    }
    catch (const std::exception & e)
    {
        SIHD_LOG(error, "CliInterpreter: {}", e.what());
        return EXIT_FAILURE;
    }
}

int CliInterpreter::_evaluate(const std::vector<std::string> & args)
{
    int help_status = this->_intercept_help(args);
    if (help_status >= 0)
        return help_status;
    int completion_status = this->_intercept_completion(args);
    if (completion_status >= 0)
        return completion_status;
    int status = this->_parse(args);
    if (status != 0)
        return status;
    if (_parse_handled)
        return status;
    if (this->_apply_set_overrides() == false)
        return EXIT_FAILURE;
    return this->_dispatch();
}

int CliInterpreter::_dispatch()
{
    Command *cur = _root.get();
    Command *child = cur->parsed_child();
    while (child)
    {
        cur = child;
        child = cur->parsed_child();
    }
    try
    {
        return cur->run();
    }
    catch (const Exit & e)
    {
        return e.status();
    }
}

CliInterpreter::TokenWalk CliInterpreter::_root_walk() const
{
    TokenWalk walk {{_root.get()}};
    walk.positionals = _root->positionals();
    return walk;
}

int CliInterpreter::_intercept_help(const std::vector<std::string> & args)
{
    TokenWalk walk = this->_root_walk();
    for (const std::string & token : args)
    {
        switch (this->_step_token(walk, token))
        {
            case TokenKind::dashes:
                return -1;
            case TokenKind::orphan:
                // in command position nothing can take the token: help owns it, CLI11 errors otherwise
                if (token == "help")
                {
                    fmt::print("{}", walk.node()->help());
                    return EXIT_SUCCESS;
                }
                return -1;
            default:
                break;
        }
    }
    return -1;
}

CliInterpreter::TokenKind CliInterpreter::_step_token(TokenWalk & walk, const std::string & token) const
{
    if (token == "--")
    {
        walk.after_dashes = true;
        return TokenKind::dashes;
    }
    if (walk.pending_value)
    {
        // a value consumed by an option is never a command
        walk.pending_value = false;
        return TokenKind::value;
    }
    if (walk.after_dashes)
        return TokenKind::positional;
    if (token.empty() || token[0] == '-')
    {
        walk.pending_value = this->_expects_next_value(walk, token);
        return TokenKind::option;
    }
    if (const Command *child = walk.node()->get_command(token))
    {
        walk.chain.push_back(child);
        walk.positionals = child->positionals();
        return TokenKind::subcommand;
    }
    // the hidden completion command is intercepted even where a positional would take it
    if (token == complete_command)
        return TokenKind::orphan;
    if (walk.positionals == -1 || walk.positionals > 0)
    {
        if (walk.positionals > 0)
            --walk.positionals;
        return TokenKind::positional;
    }
    // nothing can take this token: CLI11 owns the error
    return TokenKind::orphan;
}

bool CliInterpreter::_expects_next_value(const TokenWalk & walk, const std::string & token) const
{
    for (auto it = walk.chain.rbegin(); it != walk.chain.rend(); ++it)
    {
        if ((*it)->expects_value(token))
            return true;
    }
    return false;
}

int CliInterpreter::_intercept_completion(const std::vector<std::string> & args)
{
    TokenWalk walk = this->_root_walk();
    for (size_t i = 0; i < args.size(); ++i)
    {
        const std::string & token = args[i];
        switch (this->_step_token(walk, token))
        {
            case TokenKind::dashes:
            case TokenKind::orphan:
                // a subcommand named __complete would have descended instead
                if (token == complete_command)
                {
                    this->_complete(std::span(args.data() + i + 1, args.size() - i - 1));
                    return EXIT_SUCCESS;
                }
                return -1;
            case TokenKind::option:
            {
                std::string_view kind(token);
                if (kind.starts_with(generate_completion_flag) == false)
                    break;
                kind.remove_prefix(generate_completion_flag.size());
                bool dynamic = false;
                if (kind.empty() == false)
                {
                    // another option merely starting with the same letters
                    if (kind.starts_with('=') == false)
                        break;
                    kind.remove_prefix(1);
                    if (kind == "dynamic")
                        dynamic = true;
                    else if (kind != "static")
                    {
                        SIHD_LOG(error,
                                 "CliInterpreter: unknown completion kind '{}', expects 'static' or 'dynamic'",
                                 kind);
                        return EXIT_FAILURE;
                    }
                }
                fmt::print("{}", this->_bash_completion(dynamic));
                return EXIT_SUCCESS;
            }
            default:
                break;
        }
    }
    return -1;
}

void CliInterpreter::_complete(std::span<const std::string> words) const
{
    std::string_view partial = words.empty() ? std::string_view() : words.back();
    TokenWalk walk = this->_root_walk();
    // the last consumed word is an option whose value is being completed
    bool at_value = false;
    std::string_view value_option;
    for (size_t i = 0; i + 1 < words.size(); ++i)
    {
        const std::string & token = words[i];
        switch (this->_step_token(walk, token))
        {
            case TokenKind::dashes:
                at_value = false;
                break;
            case TokenKind::option:
                if (i + 2 == words.size() && walk.pending_value)
                {
                    at_value = true;
                    value_option = token;
                }
                break;
            default:
                break;
        }
    }

    std::vector<std::string> candidates;
    const size_t eq = partial.find('=');
    if (walk.after_dashes)
    {
        // positional values: no candidates, the shell falls back to files
    }
    else if (at_value || (partial.starts_with('-') && eq != std::string_view::npos))
    {
        std::string_view option = at_value ? value_option : partial.substr(0, eq);
        candidates = completion_values(option);
        if (at_value == false)
        {
            for (std::string & cand : candidates)
                cand.insert(0, "=").insert(0, option);
        }
    }
    else if (partial.starts_with('-'))
    {
        for (auto it = walk.chain.rbegin(); it != walk.chain.rend(); ++it)
        {
            Command::Candidates node = (*it)->candidates();
            candidates.insert(candidates.end(), node.options.begin(), node.options.end());
            candidates.insert(candidates.end(), node.flags.begin(), node.flags.end());
        }
    }
    else
    {
        candidates = walk.node()->candidates().commands;
        this->_add_help_token(walk.node(), candidates);
    }
    for (const std::string & cand : candidates)
    {
        if (str::starts_with(cand, partial))
            fmt::print("{}\n", cand);
    }
}

void CliInterpreter::_add_help_token(const Command *node, std::vector<std::string> & commands) const
{
    if (node->positionals() == 0 && node->get_command("help") == nullptr)
        commands.push_back("help");
}

void CliInterpreter::_collect_completion_lines(const Command *node,
                                               const std::string & path,
                                               const std::vector<std::string> & options,
                                               const std::vector<std::string> & flags,
                                               std::vector<std::string> & lines) const
{
    Command::Candidates own = node->candidates();
    this->_add_help_token(node, own.commands);
    std::vector<std::string> node_options = options;
    std::vector<std::string> node_flags = flags;
    node_options.insert(node_options.end(), own.options.begin(), own.options.end());
    node_flags.insert(node_flags.end(), own.flags.begin(), own.flags.end());
    lines.push_back(completion_node_line(path, own.commands, node_options, node_flags, node->positionals() != 0));
    for (const std::string & name : node->command_names())
    {
        this->_collect_completion_lines(node->get_command(name),
                                        path.empty() ? name : path + " " + name,
                                        node_options,
                                        node_flags,
                                        lines);
    }
}

std::string CliInterpreter::_bash_completion(bool dynamic) const
{
    if (_name.empty())
        return "";
    const std::string func = bash_identifier(_name);
    if (dynamic)
        return render_loader_completion(_name, func);
    std::vector<std::string> node_lines;
    this->_collect_completion_lines(_root.get(), "", {}, {}, node_lines);
    return render_static_completion(_name, func, node_lines);
}

bool CliInterpreter::_apply_set_overrides()
{
    for (const std::string & entry : _set_args)
    {
        size_t eq = entry.find('=');
        if (eq == std::string::npos)
        {
            SIHD_LOG(error, "CliInterpreter: --set expects 'command.path.key=value', got '{}'", entry);
            return false;
        }
        std::string path = entry.substr(0, eq);
        std::string value = entry.substr(eq + 1);
        size_t key_pos = path.rfind('.');
        Command *node = _root.get();
        std::string key = path;
        if (key_pos != std::string::npos)
        {
            node = _root->path(path.substr(0, key_pos));
            key = path.substr(key_pos + 1);
        }
        Command::Binding *binding = node != nullptr ? node->find_binding(key) : nullptr;
        if (binding == nullptr)
        {
            SIHD_LOG(error, "CliInterpreter: --set unknown key '{}'", path);
            return false;
        }
        if (binding->apply_string(value) == false)
        {
            SIHD_LOG(error, "CliInterpreter: --set cannot apply '{}' to '{}'", value, path);
            return false;
        }
    }
    return true;
}

} // namespace sihd::util

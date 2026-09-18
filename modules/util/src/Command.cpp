#include <cstdlib>
#include <stdexcept>

#include <fmt/format.h>

#include <sihd/util/Command.hpp>
#include <sihd/util/Logger.hpp>

#include <CLI/CLI.hpp>

namespace sihd::util
{

SIHD_LOGGER;

Command::Command(CLI::App *cli): _cli(cli)
{
    // sihd root options stay reachable from any subcommand depth
    _cli->fallthrough();
}

Command & Command::add_command(const std::string & name, const std::string & description)
{
    if (_children.find(name) != _children.end())
        throw std::invalid_argument(fmt::format("command '{}' already exists", name));
    CLI::App *sub = _cli->add_subcommand(name, description);
    _children.emplace(name, std::unique_ptr<Command>(new Command(sub)));
    return *_children[name];
}

Command & Command::add_command(const Command & other, const std::string & name)
{
    if (other._contains(this))
        throw std::logic_error(fmt::format("cannot mount command '{}' into its own subtree", other.name()));
    Command & cmd = this->add_command(name.empty() ? other.name() : name, other.description());
    for (const Binding & binding : other._bindings)
        cmd._add_binding(binding);
    cmd._on_run = other._on_run;
    for (const auto & [child_name, child] : other._children)
        cmd.add_command(*child);
    return cmd;
}

bool Command::_contains(const Command *node) const
{
    if (this == node)
        return true;
    for (const auto & [name, child] : _children)
    {
        if (child->_contains(node))
            return true;
    }
    return false;
}

bool Command::remove_command(const std::string & name)
{
    auto it = _children.find(name);
    if (it == _children.end())
        return false;
    // CLI11 owns the wrapped App: remove_subcommand destroys it
    _cli->remove_subcommand(it->second->_cli);
    _children.erase(it);
    return true;
}

bool Command::has_command(const std::string & name) const
{
    return _children.find(name) != _children.end();
}

const Command *Command::get_command(const std::string & name) const
{
    auto it = _children.find(name);
    return it == _children.end() ? nullptr : it->second.get();
}

Command *Command::get_command(const std::string & name)
{
    auto it = _children.find(name);
    return it == _children.end() ? nullptr : it->second.get();
}

const Command *Command::path(std::string_view path) const
{
    const Command *cur = this;
    for (std::string_view piece : str::split(path, '.'))
    {
        cur = cur->get_command(std::string(piece));
        if (cur == nullptr)
            return nullptr;
    }
    return cur;
}

Command *Command::path(std::string_view path)
{
    return const_cast<Command *>(const_cast<const Command *>(this)->path(path));
}

std::vector<std::string> Command::command_names() const
{
    std::vector<std::string> names;
    names.reserve(_children.size());
    for (const auto & [name, cmd] : _children)
        names.push_back(name);
    return names;
}

Command & Command::on_run(std::function<void()> fn)
{
    _on_run = std::move(fn);
    return *this;
}

int Command::run()
{
    if (_on_run)
    {
        _on_run();
        return EXIT_SUCCESS;
    }
    fmt::print("{}", this->help());
    return EXIT_SUCCESS;
}

std::string Command::name() const
{
    return _cli->get_name();
}

std::string Command::description() const
{
    return _cli->get_description();
}

std::string Command::help() const
{
    return _cli->help();
}

Command::Binding *Command::find_binding(const std::string & name)
{
    for (Binding & binding : _bindings)
    {
        if (binding.name == name)
            return &binding;
    }
    return nullptr;
}

void Command::_add_binding(Binding && binding)
{
    std::string names = binding.name;
    if (binding.positional == false)
    {
        names.clear();
        for (const std::string & alias : binding.aliases)
        {
            if (names.empty() == false)
                names += ',';
            names += alias;
        }
        if (names.empty() == false)
            names += ',';
        names += "--" + binding.name;
    }
    if (binding.flag)
        binding.option = _cli->add_flag_function(names, binding.apply_flag, binding.help);
    else if (binding.multi)
    {
        // the callback is fed all the values at once: a single string registration would keep only the first
        std::function<void(const std::string &)> apply_cli = binding.apply_cli;
        binding.option = _cli->add_option_function<std::vector<std::string>>(
            names,
            [apply_cli](const std::vector<std::string> & values) {
                for (const std::string & value : values)
                    apply_cli(value);
            },
            binding.help);
        binding.option->expected(1, -1);
    }
    else
        // a name without dashes is a positional
        binding.option = _cli->add_option_function<std::string>(names, binding.apply_cli, binding.help);
    _bindings.push_back(std::move(binding));
}

void Command::_add_binding(const Binding & binding)
{
    Binding copy = binding;
    this->_add_binding(std::move(copy));
}

Command *Command::parsed_child()
{
    std::vector<CLI::App *> subs = _cli->get_subcommands();
    if (subs.empty())
        return nullptr;
    auto it = _children.find(subs.front()->get_name());
    return it == _children.end() ? nullptr : it->second.get();
}

int Command::positionals() const
{
    int count = 0;
    for (const Binding & binding : _bindings)
    {
        if (binding.positional == false)
            continue;
        if (binding.multi)
            return -1;
        ++count;
    }
    return count;
}

Command::Candidates Command::candidates() const
{
    Candidates candidates;
    candidates.commands.reserve(_children.size());
    for (const auto & [name, child] : _children)
        candidates.commands.push_back(name);
    for (const Binding & binding : _bindings)
    {
        if (binding.positional)
            continue;
        std::vector<std::string> & list = binding.flag ? candidates.flags : candidates.options;
        list.push_back("--" + binding.name);
        list.reserve(list.size() + binding.aliases.size());
        for (const std::string & alias : binding.aliases)
            list.push_back(alias);
    }
    // the options registered without a binding: --set and --version at the interpreter root
    for (CLI::Option *opt : _cli->get_options())
    {
        bool bound = false;
        for (const Binding & binding : _bindings)
            bound = bound || binding.option == opt;
        if (bound || opt->get_positional())
            continue;
        std::vector<std::string> & list = opt->get_expected() == 0 ? candidates.flags : candidates.options;
        list.reserve(list.size() + opt->get_lnames().size() + opt->get_snames().size());
        for (const std::string & lname : opt->get_lnames())
            list.push_back("--" + lname);
        for (const std::string & sname : opt->get_snames())
            list.push_back("-" + sname);
    }
    return candidates;
}

bool Command::expects_value(const std::string & token) const
{
    if (token.size() < 2)
        return false;
    const std::vector<CLI::Option *> options = _cli->get_options();
    auto find_option = [&options](const std::string & name) -> CLI::Option * {
        for (CLI::Option *opt : options)
        {
            if (opt->check_name(name))
                return opt;
        }
        return nullptr;
    };
    if (token.starts_with("--"))
    {
        if (token.find('=') != std::string::npos)
            return false;
        CLI::Option *opt = find_option(token);
        return opt != nullptr && opt->get_expected() != 0;
    }
    // short cluster: an option takes the rest of the cluster or the next token
    for (size_t i = 1; i < token.size(); ++i)
    {
        CLI::Option *opt = find_option("-" + std::string(1, token[i]));
        if (opt == nullptr)
            return false;
        if (opt->get_expected() != 0)
            return i + 1 == token.size();
    }
    return false;
}

void Command::snapshot_cli_bindings(bool restore)
{
    for (Binding & binding : _bindings)
    {
        if (binding.option && binding.option->count() > 0)
            (restore ? binding.restore_cli : binding.snapshot_cli)();
    }
    for (auto & [name, child] : _children)
        child->snapshot_cli_bindings(restore);
}

} // namespace sihd::util

#include <sihd/util/Cli.hpp>
#include <sihd/util/File.hpp>
#include <sihd/util/LineReader.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/container.hpp>

namespace sihd::util
{

SIHD_LOGGER;

Cli::Cli(const CmdMap & map, const Splitter & splitter): _cmd_map(map), _splitter(splitter), _max_history(0) {}

void Cli::set_cmd(const std::string & cmd, CmdFun && fun)
{
    _cmd_map.emplace(cmd, std::move(fun));
}

void Cli::set_max_history(size_t max)
{
    _max_history = max;
    size_t to_remove = std::min(max, _history.size());
    _history.erase(std::prev(_history.end(), to_remove), _history.end());
}

void Cli::add_history(std::string_view line)
{
    if (_max_history == 0)
        return;
    if (_history.size() + 1 > _max_history)
    {
        _history.pop_front();
    }
    _history.emplace_back(line);
}

bool Cli::dump_history(std::string_view path) const
{
    File f(path, "a");

    if (f.is_open() == false)
        return false;

    ssize_t wrote;
    std::string to_write;

    for (const std::string & history_line : _history)
    {
        to_write = fmt::format("{}\n", history_line);
        wrote = f.write(to_write);
        if ((size_t)wrote != to_write.size())
            return false;
    }
    return true;
}

bool Cli::load_history(std::string_view path)
{
    LineReader reader(path);

    if (reader.is_open() == false)
        return false;

    Splitter history_splitter(" ");

    ArrCharView line;
    while (reader.read_next())
    {
        if (reader.get_read_data(line))
            return false;
        this->add_history(line);
    }
    return true;
}

bool Cli::handle(std::string_view line)
{
    if (line.empty())
        return false;
    if (line[line.size() - 1] == '\n')
        line.remove_suffix(1);

    std::vector<std::string> tokens = _splitter.split(line);
    if (tokens.empty())
        return false;
    std::string cmd = tokens[0];
    tokens.erase(tokens.begin());

    bool success = this->handle(cmd, tokens);
    if (success)
    {
        this->add_history(line);
    }

    return success;
}

bool Cli::handle(const std::string & cmd, const std::vector<std::string> & args)
{
    CmdFun fun = container::get_or(_cmd_map, cmd, CmdFun {nullptr});
    if (fun)
        fun(args);
    return bool(fun);
}

const std::list<std::string> Cli::history() const
{
    return _history;
}

Splitter & Cli::line_splitter()
{
    return _splitter;
}

} // namespace sihd::util

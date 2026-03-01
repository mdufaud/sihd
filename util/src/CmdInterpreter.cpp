#include <sihd/util/CmdInterpreter.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/container.hpp>

namespace sihd::util
{

SIHD_LOGGER;

CmdInterpreter::CmdInterpreter(const CmdMap & map, const Splitter & splitter):
    _cmd_map(map),
    _splitter(splitter),
    _max_history(0)
{
}

void CmdInterpreter::set_cmd(const std::string & cmd, CmdFun && fun)
{
    _cmd_map.emplace(cmd, std::move(fun));
}

void CmdInterpreter::set_max_history(size_t max)
{
    _max_history = max;
    size_t to_remove = std::min(max, _history.size());
    _history.erase(std::prev(_history.end(), to_remove), _history.end());
}

void CmdInterpreter::set_splitter(const Splitter & splitter)
{
    _splitter = splitter;
}

void CmdInterpreter::add_history(std::string_view line)
{
    if (_max_history == 0)
        return;
    if (_history.size() + 1 > _max_history)
    {
        _history.pop_front();
    }
    _history.emplace_back(line);
}

std::vector<std::string> CmdInterpreter::dump_history() const
{
    std::vector<std::string> result;
    result.reserve(_history.size());
    for (const std::string & history_line : _history)
    {
        result.push_back(history_line);
    }
    return result;
}

void CmdInterpreter::load_history(const std::vector<std::string> & lines)
{
    for (const std::string & line : lines)
    {
        this->add_history(line);
    }
}

bool CmdInterpreter::handle(std::string_view line)
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

bool CmdInterpreter::handle(const std::string & cmd, const std::vector<std::string> & args)
{
    CmdFun fun = container::get_or(_cmd_map, cmd, CmdFun {nullptr});
    if (fun)
        fun(args);
    return bool(fun);
}

const std::list<std::string> & CmdInterpreter::history() const
{
    return _history;
}

} // namespace sihd::util

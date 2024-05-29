#ifndef __SIHD_UTIL_CMDINTERPRETER_HPP__
#define __SIHD_UTIL_CMDINTERPRETER_HPP__

#include <functional>
#include <list>
#include <string>
#include <unordered_map>
#include <vector>

#include <sihd/util/Splitter.hpp>

namespace sihd::util
{

class CmdInterpreter
{
    public:
        using CmdFun = std::function<void(const std::vector<std::string> &)>;
        using CmdMap = std::unordered_map<std::string, CmdFun>;

        CmdInterpreter() = default;
        CmdInterpreter(const CmdMap & map, const Splitter & splitter = {});
        virtual ~CmdInterpreter() = default;

        void set_cmd(const std::string & cmd, CmdFun && fun);
        void set_splitter(const Splitter & splitter);

        void set_max_history(size_t max);
        bool dump_history(std::string_view path) const;
        bool load_history(std::string_view path);

        bool handle(std::string_view line);
        bool handle(const std::string & cmd, const std::vector<std::string> & args);

        const std::list<std::string> & history() const;

    protected:
        void add_history(std::string_view line);

    private:
        CmdMap _cmd_map;
        Splitter _splitter;
        std::list<std::string> _history;
        size_t _max_history;
};

} // namespace sihd::util

#endif

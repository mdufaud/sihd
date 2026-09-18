#ifndef __SIHD_UTIL_CLIINTERPRETER_HPP__
#define __SIHD_UTIL_CLIINTERPRETER_HPP__

#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/util/Command.hpp>

namespace CLI
{
class App;
}

namespace sihd::util
{

// the pure command interpreter: a command tree, the parse, the evaluations and the
// help/version/completion interceptions; evaluations are repeatable and touch no process state
class CliInterpreter
{
    public:
        // ends an evaluation with a specific status; thrown by exit(), can be thrown directly
        class Exit: public std::exception
        {
            public:
                Exit(int status): _status(status) {}

                int status() const { return _status; }

                const char *what() const noexcept override { return "cli interpreter exit"; }

            private:
                int _status;
        };

        CliInterpreter(std::string_view name, std::string_view description = "", std::string_view version = "");
        // defined in the source: CLI::App is incomplete here
        virtual ~CliInterpreter();

        Command & root() { return *_root; }

        // parse + dispatch only, without touching the process
        int evaluate(const std::vector<std::string> & args);
        // splits with quotes kept then removed before evaluation
        int evaluate(std::string_view line);
        // ends the current evaluation with this status
        [[noreturn]] void exit(int status);

        // a bash completion script snapshotting the current command tree: nothing runs at tab time
        std::string bash_completion() const;
        // a loader script querying __complete at each tab: follows a live command tree, runs the program
        std::string bash_completion_loader() const;

        const std::string & name() const { return _name; }
        const std::string & version() const { return _version; }

    protected:
        int _parse(const std::vector<std::string> & args);
        int _dispatch();
        // returns -1 when nothing was intercepted
        int _intercept_help(const std::vector<std::string> & args);
        // intercepts --generate-bash-completion and __complete; returns -1 when nothing was intercepted
        int _intercept_completion(const std::vector<std::string> & args);
        bool _apply_set_overrides();
        // set by the parse when it printed the help or the version itself
        bool _parse_handled = false;
        const std::string & log_level_conf() const { return _log_level_str; }
        void set_log_level_conf(std::string_view level) { _log_level_str = level; }
        // evaluations and boots of the derived runtimes share this lock
        std::recursive_mutex & mutex() { return _mutex; }

    private:
        // the role of a token in a command line, walked the way CLI11 consumes them
        enum class TokenKind
        {
            value,  // consumed by the previous option
            dashes, // "--": the next words are positional values
            option,
            subcommand,
            positional,
            orphan, // nothing can take it: CLI11 owns the error
        };

        // the nodes a token walk went through, the deepest being the last
        struct TokenWalk
        {
                std::vector<const Command *> chain;
                bool pending_value = false;
                bool after_dashes = false;
                int positionals = 0;

                const Command *node() const { return chain.back(); }
        };

        int _evaluate(const std::vector<std::string> & args);
        // prints the candidates for the words of the span, the last one being completed
        void _complete(std::span<const std::string> words) const;
        std::string _bash_completion(bool dynamic) const;
        TokenWalk _root_walk() const;
        // advances a walk by one token the way CLI11 would consume it
        TokenKind _step_token(TokenWalk & walk, const std::string & token) const;
        // fallthrough, the deepest match wins
        bool _expects_next_value(const TokenWalk & walk, const std::string & token) const;
        // appends the help token when the node takes no positional and owns no help command
        void _add_help_token(const Command *node, std::vector<std::string> & commands) const;
        // appends the static script case lines of the subtree, the options carrying the ancestors' along the way
        void _collect_completion_lines(const Command *node,
                                       const std::string & path,
                                       const std::vector<std::string> & options,
                                       const std::vector<std::string> & flags,
                                       std::vector<std::string> & lines) const;

        std::string _name;
        std::string _version;
        std::unique_ptr<CLI::App> _cli;
        std::unique_ptr<Command> _root;
        std::vector<std::string> _set_args;
        std::string _log_level_str;
        mutable std::recursive_mutex _mutex;
};

} // namespace sihd::util

#endif

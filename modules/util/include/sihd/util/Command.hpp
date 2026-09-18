#ifndef __SIHD_UTIL_COMMAND_HPP__
#define __SIHD_UTIL_COMMAND_HPP__

#include <concepts>
#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include <sihd/json/Json.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/traits.hpp>

namespace CLI
{
class App;
class Option;
} // namespace CLI

namespace sihd::util
{

// a node of the command tree wrapping a CLI11 App; branches live: they can be added or removed
// at any time; the interpreter drives the parses and reads the tree through this public api
class Command
{
    public:
        struct Binding
        {
                std::string name;
                // extra option names registered alongside --name, always stored dashed
                std::vector<std::string> aliases;
                std::string help;
                bool flag = false;
                bool positional = false;
                // collects every occurrence instead of keeping the last one (std::vector<std::string> binds)
                bool multi = false;
                CLI::Option *option = nullptr;
                std::function<bool(const sihd::json::Json &)> apply_json;
                std::function<bool(const std::string &)> apply_string;
                std::function<void(std::int64_t)> apply_flag;
                std::function<void(const std::string &)> apply_cli;
                std::function<void()> snapshot_cli;
                std::function<void()> restore_cli;
        };

        // what can be typed at this node: the child names and the option words
        struct Candidates
        {
                std::vector<std::string> commands; // the child names
                std::vector<std::string> options;  // options taking a value, long and short forms
                std::vector<std::string> flags;
        };

        // wraps a cli11 app: the interpreter root, or the app owned by the parent command
        // (CLI11 keeps unique_ptr children)
        explicit Command(CLI::App *cli);

        // the created branch belongs to its parent
        Command & add_command(const std::string & name, const std::string & description = "");
        // mounts a deep copy of another command (bindings, callback, whole subtree); copies keep writing
        // the bound variables and running the callbacks of the original
        Command & add_command(const Command & other, const std::string & name = "");
        bool remove_command(const std::string & name);
        bool has_command(const std::string & name) const;
        const Command *get_command(const std::string & name) const;
        Command *get_command(const std::string & name);
        // dotted path descent ("get.stat.service"), nullptr if unknown
        const Command *path(std::string_view path) const;
        Command *path(std::string_view path);
        std::vector<std::string> command_names() const;

        // the number of positional values this node takes; -1 once an unbounded positional is bound
        int positionals() const;
        // the child reached by the last parse, nullptr if none
        Command *parsed_child();
        Binding *find_binding(const std::string & name);

        // the option names of the node: long, short, and the aliases of the bindings
        Candidates candidates() const;
        // emulates CLI11 value consumption on this node options: tells if the token makes the
        // next one its value
        bool expects_value(const std::string & token) const;
        // takes or restores the values given on the command line over the whole subtree
        void snapshot_cli_bindings(bool restore);

        // binds a long option --name (or a flag for bool) to target, fed by the cli and the conf
        template <typename T>
        Command & bind(const std::string & name, T & target, const std::string & help = "")
        {
            return this->_bind(name, target, help, false);
        }

        // same bind with extra short or long option aliases ("k", "-k", "--key2")
        template <typename T, typename... Aliases>
            requires(std::convertible_to<Aliases, std::string_view> && ...)
        Command & bind(const std::string & name, T & target, const std::string & help, Aliases... aliases)
        {
            return this->_bind(name, target, help, false, {std::string(std::string_view(aliases))...});
        }

        template <typename T>
        Command & bind_positional(const std::string & name, T & target, const std::string & help = "")
        {
            return this->_bind(name, target, help, true);
        }

        // a callback ending normally succeeds; exit(status) on the captured app ends it with
        // another status, any other exception propagates out of the evaluation
        Command & on_run(std::function<void()> fn);

        // runs the on_run callback; a node without one prints its help
        int run();

        std::string name() const;
        std::string description() const;
        std::string help() const;

    private:
        // json and string conversions both need a Json::get specialization
        template <typename T>
        static constexpr bool is_bindable_v = traits::is_one_of<T,
                                                                std::string,
                                                                std::vector<std::string>,
                                                                bool,
                                                                int8_t,
                                                                uint8_t,
                                                                int16_t,
                                                                uint16_t,
                                                                int32_t,
                                                                uint32_t,
                                                                int64_t,
                                                                uint64_t,
                                                                float,
                                                                double>::value;

        template <typename T>
        Command & _bind(const std::string & name,
                        T & target,
                        const std::string & help,
                        bool positional,
                        std::vector<std::string> aliases = {})
        {
            static_assert(is_bindable_v<T>,
                          "Command: bind supports bool, fixed width integers, floats, string and vector<string>");
            Binding binding;
            binding.name = name;
            binding.aliases = std::move(aliases);
            for (std::string & alias : binding.aliases)
            {
                if (alias.empty() == false && alias.starts_with('-') == false)
                    alias.insert(0, "-");
            }
            binding.help = help;
            binding.positional = positional;
            binding.flag = std::is_same_v<T, bool>;
            binding.multi = std::is_same_v<T, std::vector<std::string>>;
            binding.apply_json = [&target](const sihd::json::Json & j) {
                try
                {
                    target = j.template get<T>();
                    return true;
                }
                catch (const std::exception &)
                {
                    return false;
                }
            };
            binding.apply_string = [&target](const std::string & str) -> bool {
                if constexpr (std::is_same_v<T, std::string>)
                {
                    target = str;
                    return true;
                }
                else if constexpr (std::is_same_v<T, std::vector<std::string>>)
                {
                    target = {str};
                    return true;
                }
                else
                {
                    std::optional<T> val = str::convert_from_string<T>(str);
                    if (val.has_value() == false)
                        return false;
                    target = *val;
                    return true;
                }
            };
            if constexpr (std::is_same_v<T, bool>)
            {
                binding.apply_flag = [&target](std::int64_t count) {
                    target = count > 0;
                };
            }
            else if constexpr (std::is_same_v<T, std::vector<std::string>>)
            {
                // each cli occurrence appends; the conf replaces the whole list
                binding.apply_cli = [&target](const std::string & str) {
                    target.push_back(str);
                };
            }
            else
            {
                binding.apply_cli = [&target, name](const std::string & str) {
                    if constexpr (std::is_same_v<T, std::string>)
                    {
                        target = str;
                    }
                    else
                    {
                        std::optional<T> val = str::convert_from_string<T>(str);
                        if (val.has_value() == false)
                            throw std::invalid_argument("cannot convert '" + str + "' for --" + name);
                        target = *val;
                    }
                };
            }
            // keeps the value given on the command line so that the conf never overrides it
            std::shared_ptr<T> snapshot = std::make_shared<T>(target);
            binding.snapshot_cli = [&target, snapshot]() {
                *snapshot = target;
            };
            binding.restore_cli = [&target, snapshot]() {
                target = *snapshot;
            };
            this->_add_binding(std::move(binding));
            return *this;
        }

        void _add_binding(Binding && binding);
        void _add_binding(const Binding & binding);
        bool _contains(const Command *node) const;

        CLI::App *_cli;
        std::function<void()> _on_run;
        std::map<std::string, std::unique_ptr<Command>> _children;
        std::list<Binding> _bindings;
};

} // namespace sihd::util

#endif

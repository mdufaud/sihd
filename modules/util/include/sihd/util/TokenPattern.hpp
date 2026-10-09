#ifndef __SIHD_UTIL_TOKENPATTERN_HPP__
#define __SIHD_UTIL_TOKENPATTERN_HPP__

#include <concepts>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/util/Error.hpp>

namespace sihd::util
{

// a compiled {name} token pattern: {{ }} escapes, optional [[fill]align][width][.precision] spec,
// else raw_spec; views into the owned pattern, move-only, a refused compile keeps the previous
class TokenPattern
{
    public:
        // std::format defaults: strings left align, space fill, no width nor truncation
        struct Options
        {
                char fill = ' ';
                char align = '<';
                size_t width = 0;
                size_t precision = 0;
        };

        struct Token
        {
                std::string_view name;
                std::string_view text;
                std::string_view raw_spec;
                Options options;
                // the spec fully matched [[fill]align][width][.precision]
                bool classical = false;
                size_t offset = 0;
        };

        TokenPattern(): _pattern(std::make_unique<std::string>()) {}

        TokenPattern(TokenPattern &&) = default;
        TokenPattern & operator=(TokenPattern &&) = default;
        TokenPattern(const TokenPattern &) = delete;
        TokenPattern & operator=(const TokenPattern &) = delete;

        std::expected<void, Error> compile(std::string_view pattern);

        const std::string & pattern() const { return *_pattern; }

        std::span<const Token> tokens() const { return _tokens; }

        size_t reserve_hint() const { return _reserve_hint; }

        template <typename Provider>
            requires(std::invocable<Provider, const Token &>)
        void render(std::string & out, Provider && provider) const
        {
            for (const Token & token : _tokens)
            {
                if (token.name.empty())
                {
                    out += token.text;
                    continue;
                }
                this->append(out, provider(token), token.options);
            }
        }

        static void append(std::string & out, std::string_view value, const Options & options);

    private:
        // heap anchor: moving the pattern transfers the buffer, the token views hold
        std::unique_ptr<std::string> _pattern;
        std::vector<Token> _tokens;
        size_t _reserve_hint = 0;
};

} // namespace sihd::util

#endif

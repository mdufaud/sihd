#ifndef __SIHD_UTIL_REGEX_HPP__
#define __SIHD_UTIL_REGEX_HPP__

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/util/Result.hpp>

namespace sihd::util
{

// PCRE2 (bytes, no UTF) compiled once, immutable and safe to share between threads
class Regex
{
    public:
        enum class Mode
        {
            search,
            full,
        };

        static Result<Regex> compile(std::string_view pattern, Mode mode = Mode::search);

        Regex(Regex && other) noexcept;
        Regex & operator=(Regex && other) noexcept;
        ~Regex();

        // a hit match/depth/heap limit is an overflow error, never a crash
        Result<bool> matches(std::string_view subject) const;
        Result<std::vector<std::string>> find_all(std::string_view subject) const;
        // replacement references groups with $1 ${1} and the whole match with $0
        Result<std::string> replace_all(std::string_view subject, std::string_view replacement) const;

    private:
        struct Impl;

        explicit Regex(std::unique_ptr<Impl> impl);

        std::unique_ptr<Impl> _impl;
};

} // namespace sihd::util

#endif

#ifndef __SIHD_UTIL_LOCATION_HPP__
#define __SIHD_UTIL_LOCATION_HPP__

namespace sihd::util
{

class Location
{
    public:
        Location(const Location &) = default;
        Location(Location &&) = default;

#if SIHD_DISABLE_LOCATION
        constexpr Location() = default;

        constexpr const char *file() const { return ""; }
        constexpr const char *function() const { return ""; }
        constexpr int line() const { return 0; }
#else
        constexpr Location(const char *file = __builtin_FILE(),
                           const char *function = __builtin_FUNCTION(),
                           int line = __builtin_LINE()):
            _file(file),
            _function(function),
            _line(line)
        {
        }

        constexpr const char *file() const { return _file; }
        constexpr const char *function() const { return _function; }
        constexpr int line() const { return _line; }

#endif

#if !SIHD_DISABLE_LOCATION

    private:
        const char *const _file;
        const char *const _function;
        const int _line;
#endif
};

} // namespace sihd::util

#endif

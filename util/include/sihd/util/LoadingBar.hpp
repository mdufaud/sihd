#ifndef __SIHD_UTIL_LOADINGBAR_HPP__
#define __SIHD_UTIL_LOADINGBAR_HPP__

#include <cstdio>
#include <string>

namespace sihd::util
{

struct LoadingBarConfiguration
{
        enum class ProgressionPos
        {
            None,
            Left,
            Right
        };

        size_t width = 30;
        size_t total = 100;
        ProgressionPos progression_pos = ProgressionPos::Right;

        char filling_char = '=';
        char remaining_char = ' ';
        char begin_bar = '[';
        char end_bar = ']';

        std::string_view progression_prefix = "";
        std::string_view progression_suffix = "";

        FILE *output = stdout;
};

class LoadingBar
{
    private:

    public:
        LoadingBar(const LoadingBarConfiguration & config);
        ~LoadingBar();

        void set_progress(size_t progress);
        void add_progress(size_t progress);
        void reset_progress() { _current = 0; }

        const LoadingBarConfiguration & config() const { return _config; }

        float progress() const { return _current / float(_config.total); }

        bool print(std::string_view to_print_before = "", std::string_view to_print_after = "") const;

    protected:
        std::string progress_str() const;
        std::string loading_bar_str() const;

        bool print_bar(std::string_view to_print_before, std::string_view to_print_after) const;

    private:
        size_t _current;
        LoadingBarConfiguration _config;
};

} // namespace sihd::util

#endif
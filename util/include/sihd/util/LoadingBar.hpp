#ifndef __SIHD_UTIL_LOADINGBAR_HPP__
#define __SIHD_UTIL_LOADINGBAR_HPP__

#include <cstdio>
#include <string>

namespace sihd::util
{

class LoadingBar
{
    private:
        enum InternalPercentPos
        {
            None,
            Left,
            Right
        };

    public:
        LoadingBar(size_t width = 10, size_t total = 0, FILE *output = stdout);
        ~LoadingBar();

        void set_percent_pos_left() { _percent_pos = Left; }
        void set_percent_pos_right() { _percent_pos = Right; };
        void set_width(size_t width) { _width = width; }
        void set_total(size_t total) { _total = total; }

        void add_progress(size_t progress);
        void reset_progress() { _current = 0; }

        size_t width() const { return _width; }
        size_t total() const { return _total; }
        float progress() const { return _current / float(_total); }

        bool print() const;

    protected:
        std::string progress_str() const;
        std::string loading_bar_str() const;

        bool print_bar() const;

    private:
        size_t _width;
        size_t _current;
        size_t _total;

        FILE *_file_ptr;

        InternalPercentPos _percent_pos;
};

} // namespace sihd::util

#endif
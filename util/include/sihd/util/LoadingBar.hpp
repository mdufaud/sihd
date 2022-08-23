#ifndef __SIHD_UTIL_LOADINGBAR_HPP__
# define __SIHD_UTIL_LOADINGBAR_HPP__

# include <string>

# include <stdio.h>

namespace sihd::util
{

class LoadingBar
{
    private:
        enum InternalPercentPos
        {
            none,
            left,
            right
        };

    public:
        LoadingBar(size_t width = 10, size_t total = 0, FILE *output = stdout);
        virtual ~LoadingBar();

        void set_percent_pos_left() { _percent_pos = left; }
        void set_percent_pos_right() { _percent_pos = right; };
        void set_width(size_t width) { _width = width; }
        void set_total(size_t total) { _total = total; }

        bool add_progress(size_t progress);
        void reset_progress() { _current = 0; }

        size_t width() const { return _width; }
        size_t total() const { return _total; }
        float progress() const { return _current / float(_total); }

    protected:
        std::string progress_str() const;
        std::string loading_bar_str() const;

        virtual bool _return_begin_of_line() const;
        virtual bool _print_bar() const;
        virtual bool _flush() const;

    private:
        bool _print(std::string_view str) const;

        size_t _width;
        size_t _current;
        size_t _total;

        FILE *_file_ptr;

        InternalPercentPos _percent_pos;
};

}

#endif
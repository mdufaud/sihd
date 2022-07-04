#include <fmt/format.h>

#include <sihd/util/LoadingBar.hpp>

namespace sihd::util
{

LoadingBar::LoadingBar(size_t width, size_t total, FILE *output):
    _width(width), _current(0), _total(total), _file_ptr(output), _percent_pos(none)
{
}

LoadingBar::~LoadingBar()
{
}

bool    LoadingBar::add_progress(size_t progress)
{
    _current += progress;
    _current = std::min(_current, _total);
    return this->_return_begin_of_line()
            && this->_print_bar()
            && this->_flush();
}

bool    LoadingBar::_return_begin_of_line() const
{
    return this->_print("\r");
}

bool    LoadingBar::_print_bar() const
{
    std::string bar_str = this->loading_bar_str();

    if (_percent_pos == left)
        bar_str = fmt::format("{} {}", this->progress_str(), bar_str);
    else if (_percent_pos == right)
        bar_str = fmt::format("{} {}", bar_str, this->progress_str());

    return this->_print(bar_str);
}

bool    LoadingBar::_flush() const
{
    return fflush(_file_ptr) == 0;
}

bool    LoadingBar::_print(std::string_view str) const
{
    return fwrite(str.data(), sizeof(char), str.size(), _file_ptr) == str.size();
}

std::string LoadingBar::loading_bar_str() const
{
    float progress = this->progress();
    progress = std::max(0.0f, progress);
    progress = std::min(progress, 1.0f);
    const int current = _width * progress;
    const int remaining = _width - current;
    return fmt::format("[{}{}]",
            fmt::format("{:=<{}}", "", current),
            fmt::format("{:<{}}", "", remaining));
}

std::string LoadingBar::progress_str() const
{
    return fmt::format("{:<3.0f}%", this->progress() * 100.f);
}

}
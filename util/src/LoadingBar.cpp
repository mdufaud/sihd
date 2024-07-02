#include <fmt/format.h>

#include <sihd/util/LoadingBar.hpp>

namespace sihd::util
{

namespace
{

bool do_print(FILE *file_ptr, std::string_view str)
{
    return fwrite(str.data(), sizeof(char), str.size(), file_ptr) == str.size();
}

bool do_flush(FILE *file_ptr)
{
    return fflush(file_ptr) == 0;
}

bool do_return_begin_of_line(FILE *file_ptr)
{
    return do_print(file_ptr, "\r");
}

} // namespace

LoadingBar::LoadingBar(size_t width, size_t total, FILE *output):
    _width(width),
    _current(0),
    _total(total),
    _file_ptr(output),
    _percent_pos(None)
{
}

LoadingBar::~LoadingBar() {}

void LoadingBar::add_progress(size_t progress)
{
    _current += progress;
    _current = std::min(_current, _total);
}

bool LoadingBar::print() const
{
    return do_return_begin_of_line(_file_ptr) && this->print_bar() && do_flush(_file_ptr);
}

bool LoadingBar::print_bar() const
{
    std::string bar_str = this->loading_bar_str();

    if (_percent_pos == Left)
        bar_str = fmt::format("{} {}", this->progress_str(), bar_str);
    else if (_percent_pos == Right)
        bar_str = fmt::format("{} {}", bar_str, this->progress_str());

    return do_print(_file_ptr, bar_str);
}

std::string LoadingBar::loading_bar_str() const
{
    float progress = this->progress();
    progress = std::max(0.0f, progress);
    progress = std::min(progress, 1.0f);
    const int current = _width * progress;
    const int remaining = _width - current;
    return fmt::format("[{}{}]", fmt::format("{:=<{}}", "", current), fmt::format("{:<{}}", "", remaining));
}

std::string LoadingBar::progress_str() const
{
    return fmt::format("{:<3.0f}%", this->progress() * 100.f);
}

} // namespace sihd::util
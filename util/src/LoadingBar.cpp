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

LoadingBar::LoadingBar(const LoadingBarConfiguration & config): _current(0), _config(config) {}

LoadingBar::~LoadingBar() = default;

void LoadingBar::set_progress(size_t progress)
{
    _current = progress;
}

void LoadingBar::add_progress(size_t progress)
{
    this->set_progress(std::min(_current + progress, _config.total));
}

bool LoadingBar::print(std::string_view to_print_before, std::string_view to_print_after) const
{
    return do_return_begin_of_line(_config.output) && this->print_bar(to_print_before, to_print_after)
           && do_flush(_config.output);
}
bool LoadingBar::print_bar(std::string_view to_print_before, std::string_view to_print_after) const
{
    std::string bar = this->loading_bar_str();
    std::string progress_str = this->progress_str();

    std::string bar_str;
    if (_config.progression_pos == LoadingBarConfiguration::ProgressionPos::Left)
        bar_str = fmt::format("{} {}{}{}", progress_str, to_print_before, bar, to_print_after);
    else if (_config.progression_pos == LoadingBarConfiguration::ProgressionPos::Right)
        bar_str = fmt::format("{}{} {}{}", to_print_before, bar, progress_str, to_print_after);
    else
        bar_str = fmt::format("{}{}{}", to_print_before, bar, to_print_after);

    return do_print(_config.output, bar_str);
}

std::string LoadingBar::loading_bar_str() const
{
    float progress = this->progress();
    progress = std::max(0.0f, progress);
    progress = std::min(progress, 1.0f);
    const int current = _config.width * progress;
    const int remaining = _config.width - current;

    return fmt::format("{}{}{}{}",
                       _config.begin_bar,
                       std::string(current, _config.filling_char),
                       std::string(remaining, _config.remaining_char),
                       _config.end_bar);
}

std::string LoadingBar::progress_str() const
{
    return fmt::format("{}{:<3.0f}{}",
                       _config.progression_prefix,
                       this->progress() * 100.f,
                       _config.progression_suffix);
}

} // namespace sihd::util
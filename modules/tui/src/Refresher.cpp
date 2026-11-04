#include <sihd/tui/Refresher.hpp>

namespace sihd::tui
{

Refresher::Refresher(std::function<void()> post): _post(std::move(post)), _pending(false) {}

void Refresher::request()
{
    if (_pending.exchange(true) == false)
        _post();
}

void Refresher::rendered()
{
    _pending = false;
}

ftxui::Component RefreshOnRender(ftxui::Component child, Refresher & refresher)
{
    return ftxui::Renderer(child, [child, &refresher] {
        refresher.rendered();
        return child->Render();
    });
}

} // namespace sihd::tui

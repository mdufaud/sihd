#ifndef __SIHD_TUI_REFRESHER_HPP__
#define __SIHD_TUI_REFRESHER_HPP__

#include <atomic>
#include <functional>

#include <ftxui/component/component.hpp>

namespace sihd::tui
{

class Refresher
{
    public:
        explicit Refresher(std::function<void()> post);

        void request();
        void rendered();

    private:
        std::function<void()> _post;
        std::atomic<bool> _pending;
};

ftxui::Component RefreshOnRender(ftxui::Component child, Refresher & refresher);

} // namespace sihd::tui

#endif

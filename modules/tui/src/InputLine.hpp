#ifndef __SIHD_TUI_INPUTLINE_HPP__
#define __SIHD_TUI_INPUTLINE_HPP__

#include <functional>
#include <string>

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/dom/elements.hpp>

namespace sihd::tui
{

// Single line input: a ftxui Input kept for its event handling, drawn here.
//
// ftxui draws no cursor when the Input content is empty: it wraps the placeholder into a focus node
// which overrides the cursor node, sending the terminal cursor to the right edge of the field. The
// line is drawn here instead, the terminal cursor sitting on the cell left of the caret - hence the
// prompt, which gives that cell a place to be when the content is empty.
class InputLine: public ftxui::ComponentBase
{
    public:
        InputLine(std::string prompt, std::string placeholder);
        ~InputLine() = default;

        [[nodiscard]] const std::string & content() const { return _content; }
        // sets the content and moves the caret to its end
        void set_content(std::string content);
        void clear();

        std::function<void()> on_enter;
        std::function<void()> on_change;

    private:
        ftxui::Element OnRender() override;
        ftxui::Component ActiveChild() override { return _input; }

        std::string _prompt;
        std::string _placeholder;
        std::string _content;
        int _cursor_pos = 0;
        ftxui::Component _input;
};

} // namespace sihd::tui

#endif

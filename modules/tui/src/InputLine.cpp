#include <ftxui/component/component_options.hpp>

#include "InputLine.hpp"

namespace sihd::tui
{

using namespace ftxui;

namespace
{

// start of the utf-8 glyph ending at pos (ftxui's GlyphPrevious is an internal header)
size_t glyph_before(const std::string & str, size_t pos)
{
    if (pos == 0)
        return 0;
    --pos;
    while (pos > 0 && ((unsigned char)str[pos] & 0xC0) == 0x80)
        --pos;
    return pos;
}

} // namespace

InputLine::InputLine(std::string prompt, std::string placeholder):
    _prompt(std::move(prompt)),
    _placeholder(std::move(placeholder))
{
    InputOption options;
    options.content = &_content;
    options.cursor_position = &_cursor_pos;
    options.multiline = false;
    options.on_enter = [this] {
        if (on_enter)
            on_enter();
    };
    // only typing triggers this: set_content does not
    options.on_change = [this] {
        if (on_change)
            on_change();
    };
    _input = Input(options);
    Add(_input);
}

void InputLine::set_content(std::string content)
{
    _content = std::move(content);
    _cursor_pos = (int)_content.size();
}

void InputLine::clear()
{
    _content.clear();
    _cursor_pos = 0;
}

Element InputLine::OnRender()
{
    Elements parts;
    if (!_prompt.empty())
        parts.push_back(text(_prompt));

    if (!Focused())
    {
        parts.push_back(_content.empty() ? text(_placeholder) | dim : text(_content));
        parts.push_back(filler());
        return hbox(std::move(parts));
    }

    const size_t caret = std::min((size_t)std::max(_cursor_pos, 0), _content.size());
    const size_t block_start = glyph_before(_content, caret);

    if (caret == 0)
    {
        // nothing on the left of the caret: the block falls back on the last cell of the prompt
        if (_prompt.empty())
            parts.push_back(text(" ") | focusCursorBlockBlinking);
        else
        {
            parts.pop_back();
            const size_t prompt_start = glyph_before(_prompt, _prompt.size());
            parts.push_back(text(_prompt.substr(0, prompt_start)));
            parts.push_back(text(_prompt.substr(prompt_start)) | focusCursorBlockBlinking);
        }
        parts.push_back(text(_content));
    }
    else
    {
        parts.push_back(text(_content.substr(0, block_start)));
        parts.push_back(text(_content.substr(block_start, caret - block_start)) | focusCursorBlockBlinking);
        parts.push_back(text(_content.substr(caret)));
    }

    parts.push_back(filler());
    return hbox(std::move(parts)) | xframe;
}

} // namespace sihd::tui

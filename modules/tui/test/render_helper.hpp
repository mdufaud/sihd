#ifndef __SIHD_TUI_TEST_RENDER_HELPER_HPP__
#define __SIHD_TUI_TEST_RENDER_HELPER_HPP__

#include <string>
#include <vector>

#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>

namespace test
{

inline std::vector<std::string> render_lines(ftxui::Element element, int width, int height)
{
    ftxui::Screen screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(width), ftxui::Dimension::Fixed(height));
    ftxui::Render(screen, element);
    std::vector<std::string> lines;
    for (int y = 0; y < height; ++y)
    {
        std::string line;
        for (int x = 0; x < width; ++x)
            line += screen.PixelAt(x, y).character.empty() ? " " : screen.PixelAt(x, y).character;
        line.erase(line.find_last_not_of(' ') + 1);
        lines.push_back(line);
    }
    return lines;
}

} // namespace test

#endif

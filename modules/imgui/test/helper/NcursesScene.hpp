#ifndef __SIHD_IMGUI_TEST_NCURSESSCENE_HPP__
#define __SIHD_IMGUI_TEST_NCURSESSCENE_HPP__

#include <imgui.h>

#include <functional>
#include <string>

#include <gtest/gtest.h>

#include <sihd/imgui/ImguiRendererNcurses.hpp>

namespace test
{

class NcursesScene: public ::testing::Test
{
    protected:
        // cell_size: imgui units per terminal cell, width and height stay in cells
        NcursesScene(int width, int height, ImVec2 cell_size = ImVec2(1.0f, 1.0f));
        ~NcursesScene() override;

        void render_frames(int count, const std::function<void()> & build);
        void render_scene(const std::function<void()> & content);
        void scene_window(const std::function<void()> & content) const;

        const sihd::imgui::TCell & cell(int x, int y) const;
        std::string row_text(int y) const;
        std::string screen_text() const;
        std::string row_backgrounds(int y) const;
        int first_col_with_text(int y, const std::string & text) const;

        const int width;
        const int height;
        const ImVec2 cell_size;
        ImVec4 clear_color;
        sihd::imgui::ImguiRendererNcurses renderer;
        ImGuiContext *ctx = nullptr;
};

} // namespace test

#endif

#include <imgui.h>

#include <initializer_list>

#include <gtest/gtest.h>

#include "helper/NcursesScene.hpp"

namespace test
{

class TestNcursesImage: public NcursesScene
{
    protected:
        TestNcursesImage(): NcursesScene(24, 8) {}

        void load_rgba(int w, int h, std::initializer_list<ImU32> pixels)
        {
            texture.Create(ImTextureFormat_RGBA32, w, h);
            int byte = 0;
            for (ImU32 pixel : pixels)
            {
                for (int shift = 0; shift <= IM_COL32_A_SHIFT; shift += 8)
                    texture.Pixels[byte++] = (unsigned char)(pixel >> shift);
            }
        }

        ImTextureData texture;
};

TEST_F(TestNcursesImage, pixels_are_drawn_as_half_blocks)
{
    load_rgba(2,
              4,
              {IM_COL32(255, 0, 0, 255),
               IM_COL32(0, 255, 0, 255),
               IM_COL32(0, 0, 255, 255),
               IM_COL32(255, 255, 255, 255),
               IM_COL32(0, 0, 0, 255),
               IM_COL32(255, 255, 0, 255),
               IM_COL32(0, 255, 255, 255),
               IM_COL32(255, 0, 255, 255)});

    render_scene([this] { ImGui::Image(texture.GetTexRef(), ImVec2(2, 2)); });

    EXPECT_EQ(row_text(1), "│▀▀                    │");
    EXPECT_EQ(row_text(2), "│▀▀                    │");
    EXPECT_EQ(cell(1, 1).fg, IM_COL32(255, 0, 0, 0));
    EXPECT_EQ(cell(1, 1).bg, IM_COL32(0, 0, 255, 0));
    EXPECT_EQ(cell(2, 1).fg, IM_COL32(0, 255, 0, 0));
    EXPECT_EQ(cell(2, 1).bg, IM_COL32(255, 255, 255, 0));
    EXPECT_EQ(cell(1, 2).fg, IM_COL32(0, 0, 0, 0));
    EXPECT_EQ(cell(1, 2).bg, IM_COL32(0, 255, 255, 0));
    EXPECT_EQ(cell(2, 2).fg, IM_COL32(255, 255, 0, 0));
    EXPECT_EQ(cell(2, 2).bg, IM_COL32(255, 0, 255, 0));
}

TEST_F(TestNcursesImage, downscaled_pixels_are_averaged)
{
    load_rgba(4,
              2,
              {IM_COL32(0, 0, 0, 255),
               IM_COL32(0, 0, 0, 255),
               IM_COL32(255, 255, 255, 255),
               IM_COL32(255, 255, 255, 255),
               IM_COL32(255, 0, 0, 255),
               IM_COL32(255, 0, 0, 255),
               IM_COL32(0, 0, 255, 255),
               IM_COL32(0, 0, 255, 255)});

    render_scene([this] { ImGui::Image(texture.GetTexRef(), ImVec2(1, 1)); });

    EXPECT_EQ(cell(1, 1).fg, IM_COL32(128, 128, 128, 0));
    EXPECT_EQ(cell(1, 1).bg, IM_COL32(128, 0, 128, 0));
}

TEST_F(TestNcursesImage, upscaled_pixel_fills_every_cell_it_covers)
{
    load_rgba(1, 1, {IM_COL32(255, 0, 0, 255)});

    render_scene([this] { ImGui::Image(texture.GetTexRef(), ImVec2(3, 2)); });

    EXPECT_EQ(row_text(1), "│▀▀▀                   │");
    EXPECT_EQ(row_text(2), "│▀▀▀                   │");
    EXPECT_EQ(cell(3, 2).fg, IM_COL32(255, 0, 0, 0));
    EXPECT_EQ(cell(3, 2).bg, IM_COL32(255, 0, 0, 0));
}

TEST_F(TestNcursesImage, transparent_pixel_shows_what_lies_below)
{
    load_rgba(1, 2, {IM_COL32(255, 255, 255, 0), IM_COL32(255, 0, 0, 255)});

    render_scene([this] { ImGui::Image(texture.GetTexRef(), ImVec2(1, 1)); });

    EXPECT_EQ(cell(1, 1).fg, cell(5, 1).bg);
    EXPECT_EQ(cell(1, 1).bg, IM_COL32(255, 0, 0, 0));
}

TEST_F(TestNcursesImage, tint_colours_the_pixels)
{
    load_rgba(1, 2, {IM_COL32(255, 255, 255, 255), IM_COL32(128, 128, 128, 255)});

    render_scene([this] {
        ImGui::ImageWithBg(texture.GetTexRef(),
                           ImVec2(1, 1),
                           ImVec2(0, 0),
                           ImVec2(1, 1),
                           ImVec4(0, 0, 0, 0),
                           ImVec4(1, 0, 0, 1));
    });

    EXPECT_EQ(cell(1, 1).fg, IM_COL32(255, 0, 0, 0));
    EXPECT_EQ(cell(1, 1).bg, IM_COL32(128, 0, 0, 0));
}

TEST_F(TestNcursesImage, swapped_uvs_mirror_the_image)
{
    load_rgba(
        2,
        2,
        {IM_COL32(255, 0, 0, 255), IM_COL32(0, 255, 0, 255), IM_COL32(0, 0, 255, 255), IM_COL32(255, 255, 255, 255)});

    render_scene([this] { ImGui::Image(texture.GetTexRef(), ImVec2(2, 1), ImVec2(1, 1), ImVec2(0, 0)); });

    EXPECT_EQ(cell(1, 1).fg, IM_COL32(255, 255, 255, 0));
    EXPECT_EQ(cell(1, 1).bg, IM_COL32(0, 255, 0, 0));
    EXPECT_EQ(cell(2, 1).fg, IM_COL32(0, 0, 255, 0));
    EXPECT_EQ(cell(2, 1).bg, IM_COL32(255, 0, 0, 0));
}

TEST_F(TestNcursesImage, alpha_texture_is_white_coverage)
{
    texture.Create(ImTextureFormat_Alpha8, 1, 2);
    texture.Pixels[0] = 255;
    texture.Pixels[1] = 0;

    render_scene([this] { ImGui::Image(texture.GetTexRef(), ImVec2(1, 1)); });

    EXPECT_EQ(cell(1, 1).fg, IM_COL32(255, 255, 255, 0));
    EXPECT_EQ(cell(1, 1).bg, cell(5, 1).bg);
}

TEST_F(TestNcursesImage, ascii_screen_mixes_both_halves_in_the_background)
{
    renderer.set_unicode(false);
    load_rgba(1, 2, {IM_COL32(255, 0, 0, 255), IM_COL32(0, 0, 255, 255)});

    render_scene([this] { ImGui::Image(texture.GetTexRef(), ImVec2(1, 1)); });

    EXPECT_EQ(row_text(1), "|                      |");
    EXPECT_EQ(cell(1, 1).bg, IM_COL32(128, 0, 128, 0));
}

TEST_F(TestNcursesImage, empty_texture_is_a_shaded_block)
{
    texture.Create(ImTextureFormat_RGBA32, 0, 0);

    render_scene([this] { ImGui::Image(texture.GetTexRef(), ImVec2(2, 1)); });

    EXPECT_EQ(row_text(1), "│▒▒                    │");
}

} // namespace test

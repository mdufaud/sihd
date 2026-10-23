#include <imgui.h>

#include <array>
#include <clocale>
#include <functional>
#include <string>

#include <gtest/gtest.h>

#include "helper/NcursesScene.hpp"

namespace test
{

class TestNcursesScene: public NcursesScene
{
    protected:
        TestNcursesScene(): NcursesScene(16, 5), saved_locale(std::setlocale(LC_CTYPE, nullptr)) {}
        ~TestNcursesScene() override { std::setlocale(LC_CTYPE, saved_locale.c_str()); }

        void input_scene(std::array<char, 16> & text)
        {
            this->scene_window([&] {
                ImGui::SetKeyboardFocusHere();
                ImGui::InputText("##in", text.data(), text.size());
            });
        }

        void buttons_scene() const
        {
            this->scene_window([] {
                ImGui::Button("A");
                ImGui::Button("B");
            });
        }

        void click_scene(bool & clicked) const
        {
            this->scene_window([&] { clicked |= ImGui::Button("Click"); });
        }

        void selectables_scene() const
        {
            this->scene_window([] {
                ImGui::Selectable("one");
                ImGui::Selectable("two");
            });
        }

        void tall_multi_select_scene() const
        {
            this->scene_window([] {
                ImGui::BeginMultiSelect(ImGuiMultiSelectFlags_None, 0, 1);
                ImGui::SetNextItemSelectionUserData(0);
                ImGui::Selectable("tall", false, ImGuiSelectableFlags_None, ImVec2(6, 3));
                ImGui::EndMultiSelect();
            });
        }

        void press_tab(const std::function<void()> & scene)
        {
            ImGuiIO & io = ImGui::GetIO();
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
            render_frames(2, scene);
            io.AddKeyEvent(ImGuiKey_Tab, true);
            render_frames(1, scene);
            io.AddKeyEvent(ImGuiKey_Tab, false);
            render_frames(1, scene);
        }

        std::string row_reverse(int y) const
        {
            std::string flags;
            for (int x = 0; x < width; ++x)
                flags += this->cell(x, y).reverse ? 'r' : '.';
            return flags;
        }

        const std::string saved_locale;
};

TEST_F(TestNcursesScene, text_sits_inside_the_window_frame)
{
    render_scene([] { ImGui::TextUnformatted("caf\xc3\xa9 25\xc2\xb0 a_b"); });

    EXPECT_EQ(screen_text(),
              "┌──────────────┐\n"
              "│café 25° a_b  │\n"
              "│              │\n"
              "│              │\n"
              "└──────────────┘");
}

TEST_F(TestNcursesScene, ascii_mode_draws_the_frame_with_ascii)
{
    renderer.set_unicode(false);

    render_scene([] { ImGui::TextUnformatted("text"); });

    EXPECT_EQ(screen_text(),
              "+--------------+\n"
              "|text          |\n"
              "|              |\n"
              "|              |\n"
              "+--------------+");
}

TEST_F(TestNcursesScene, wide_glyph_takes_two_cells)
{
    if (std::setlocale(LC_CTYPE, "C.UTF-8") == nullptr)
        GTEST_SKIP() << "C.UTF-8 locale unavailable";

    render_scene([] {
        ImGui::TextUnformatted("a\xe6\xbc\xa2"
                               "b");
    });

    EXPECT_EQ(row_text(1), "│a漢b          │");
    EXPECT_TRUE(cell(3, 1).wide_tail);
}

TEST_F(TestNcursesScene, input_caret_keeps_character)
{
    std::array<char, 16> text {"abc"};
    ImGuiIO & io = ImGui::GetIO();

    render_frames(2, [&] { input_scene(text); });
    io.AddKeyEvent(ImGuiKey_LeftArrow, true);
    render_frames(1, [&] { input_scene(text); });
    io.AddKeyEvent(ImGuiKey_LeftArrow, false);
    render_frames(1, [&] { input_scene(text); });

    EXPECT_EQ(row_text(1), "│ abc          │");
}

TEST_F(TestNcursesScene, selected_text_is_reverse_video)
{
    std::array<char, 16> text {"abc"};
    ImGuiIO & io = ImGui::GetIO();

    render_frames(2, [&] { input_scene(text); });
    io.AddKeyEvent(ImGuiMod_Ctrl, true);
    io.AddKeyEvent(ImGuiKey_A, true);
    render_frames(1, [&] { input_scene(text); });
    io.AddKeyEvent(ImGuiKey_A, false);
    io.AddKeyEvent(ImGuiMod_Ctrl, false);
    render_frames(1, [&] { input_scene(text); });

    EXPECT_EQ(row_text(1), "│ abc          │");
    EXPECT_EQ(row_reverse(1), "..rrr...........");
}

TEST_F(TestNcursesScene, keyboard_focus_is_reverse_video)
{
    press_tab([&] { buttons_scene(); });

    EXPECT_EQ(row_reverse(1), ".rrr............");
    EXPECT_EQ(row_reverse(2), "................");
}

TEST_F(TestNcursesScene, keyboard_focus_on_selectable_reverses_its_row_only)
{
    press_tab([&] { selectables_scene(); });

    EXPECT_EQ(row_reverse(1), ".rrrrrrrrrrrrrr.");
    EXPECT_EQ(row_reverse(2), "................");
}

TEST_F(TestNcursesScene, keyboard_focus_on_tall_selectable_reverses_the_whole_item)
{
    press_tab([&] { tall_multi_select_scene(); });

    EXPECT_EQ(row_reverse(1), ".rrrrrrr........");
    EXPECT_EQ(row_reverse(2), ".rrrrrrr........");
    EXPECT_EQ(row_reverse(3), ".rrrrrrr........");
    EXPECT_EQ(row_reverse(4), "................");
}

TEST_F(TestNcursesScene, disabled_button_label_is_dimmer_than_enabled)
{
    render_scene([] {
        ImGui::Button("A");
        ImGui::BeginDisabled();
        ImGui::Button("A");
        ImGui::EndDisabled();
    });

    EXPECT_EQ(cell(2, 1).fg, 0xFFFFFFu);
    EXPECT_EQ(cell(2, 2).fg, 0xBCB2AAu);
}

TEST_F(TestNcursesScene, click_on_first_button_cell_triggers_it)
{
    ImGuiIO & io = ImGui::GetIO();
    bool clicked = false;

    render_frames(2, [&] { click_scene(clicked); });
    io.AddMousePosEvent(1.0f, 1.0f);
    render_frames(1, [&] { click_scene(clicked); });
    io.AddMouseButtonEvent(0, true);
    render_frames(1, [&] { click_scene(clicked); });
    io.AddMouseButtonEvent(0, false);
    render_frames(1, [&] { click_scene(clicked); });

    EXPECT_EQ(row_backgrounds(1), "abbbbbbbaaaaaaaa");
    EXPECT_TRUE(clicked);
}

} // namespace test

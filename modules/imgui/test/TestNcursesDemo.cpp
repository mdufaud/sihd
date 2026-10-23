#include <imgui.h>
#include <imgui_internal.h>

#include <array>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <sihd/sys/env.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/build.hpp>
#include <sihd/util/str.hpp>

#include "helper/NcursesScene.hpp"

namespace test
{

using namespace sihd::imgui;
using namespace sihd::sys;
using namespace sihd::util;

struct DemoCase
{
        const char *name;
        // nullptr: a section header of the demo window, else the menu holding the window's item
        const char *menu;
        const char *label;
        // a section's screen height: its rows with every tree node open
        int rows = 0;
};

void PrintTo(const DemoCase & demo, std::ostream *os)
{
    *os << demo.name;
}

namespace
{

constexpr int demo_width = 120;
constexpr int window_height = 50;
constexpr int settle_frames = 6;
// logging to a buffer opens every tree node down to this depth
constexpr int log_open_depth = 99;
constexpr const char *demo_window = "Dear ImGui Demo";
constexpr const char *update_variable = "SIHD_IMGUI_GOLDEN_UPDATE";
constexpr const char *golden_set = "demo_8x16";
constexpr ImVec2 cell_units {8.0f, 16.0f};

constexpr std::array<DemoCase, 28> demo_cases = {{
    {"section_help", nullptr, "Help", 60},
    {"section_configuration", nullptr, "Configuration", 76},
    {"section_window_options", nullptr, "Window options", 24},
    {"section_widgets", nullptr, "Widgets", 1602},
    {"section_layout", nullptr, "Layout & Scrolling", 244},
    {"section_popups", nullptr, "Popups & Modal windows", 46},
    {"section_tables", nullptr, "Tables & Columns", 630},
    {"section_inputs", nullptr, "Inputs & Focus", 126},
    {"example_main_menu_bar", "Examples", "Main menu bar"},
    {"example_assets_browser", "Examples", "Assets Browser"},
    {"example_console", "Examples", "Console"},
    {"example_custom_rendering", "Examples", "Custom rendering"},
    {"example_documents", "Examples", "Documents"},
    {"example_image_viewer", "Examples", "Image Viewer"},
    {"example_log", "Examples", "Log"},
    {"example_property_editor", "Examples", "Property editor"},
    {"example_simple_layout", "Examples", "Simple layout"},
    {"example_simple_overlay", "Examples", "Simple overlay"},
    {"example_auto_resizing", "Examples", "Auto-resizing window"},
    {"example_constrained_resizing", "Examples", "Constrained-resizing window"},
    {"example_fullscreen", "Examples", "Fullscreen window"},
    {"example_long_text", "Examples", "Long text display"},
    {"example_window_titles", "Examples", "Manipulating window titles"},
    {"tool_metrics", "Tools", "Metrics/Debugger"},
    {"tool_debug_log", "Tools", "Debug Log"},
    {"tool_id_stack", "Tools", "ID Stack Tool"},
    {"tool_style_editor", "Tools", "Style Editor"},
    {"tool_about", "Tools", "About Dear ImGui"},
}};

bool is_word_char(uint32_t ch)
{
    return ch < 0x80 && (std::isalnum((int)ch) != 0);
}

// FNV-1a over the four bytes of a value: the same hash on every architecture
uint32_t hash_mix(uint32_t hash, uint32_t value)
{
    constexpr uint32_t fnv_prime = 16777619u;
    for (int shift = 0; shift < 32; shift += 8)
        hash = (hash ^ ((value >> shift) & 0xFFu)) * fnv_prime;
    return hash;
}

uint32_t row_style_hash(const ImguiRendererNcurses & renderer, int width, int y)
{
    constexpr uint32_t fnv_offset = 2166136261u;
    uint32_t hash = fnv_offset;
    for (int x = 0; x < width; ++x)
    {
        const TCell & cell = renderer.get_cell(x, y);
        const bool glyph = cell.ch != 0 && cell.ch != ' ';
        hash = hash_mix(hash, cell.has_bg ? cell.bg : UINT32_MAX);
        hash = hash_mix(hash, (glyph || cell.reverse || cell.underline) ? cell.fg : UINT32_MAX);
        hash = hash_mix(hash, (cell.dim ? 1u : 0u) | (cell.reverse ? 2u : 0u) | (cell.underline ? 4u : 0u));
    }
    return hash;
}

// %p prints lowercase hex whose value and length change with each run and architecture; ImGuiID prints uppercase
bool contains_pointer(std::string_view text)
{
    constexpr size_t min_pointer_digits = 6;
    for (size_t pos = text.find("0x"); pos != std::string_view::npos; pos = text.find("0x", pos + 2))
    {
        size_t digits = 0;
        while (pos + 2 + digits < text.size() && std::isxdigit((unsigned char)text[pos + 2 + digits]) != 0
               && std::isupper((unsigned char)text[pos + 2 + digits]) == 0)
            ++digits;
        if (digits >= min_pointer_digits)
            return true;
    }
    return false;
}

bool is_empty_row(std::string text)
{
    for (const std::string_view border : {"│", "└", "┘", "─", "◢", " "})
    {
        for (size_t pos = text.find(border); pos != std::string::npos; pos = text.find(border))
            text.erase(pos, border.size());
    }
    return text.empty();
}

std::string first_difference(const std::string & expected, const std::string & actual)
{
    const std::vector<std::string> expected_lines = str::split(expected, "\n");
    const std::vector<std::string> actual_lines = str::split(actual, "\n");
    for (size_t line = 0; line < std::max(expected_lines.size(), actual_lines.size()); ++line)
    {
        const std::string & want = line < expected_lines.size() ? expected_lines[line] : "<end of file>";
        const std::string & got = line < actual_lines.size() ? actual_lines[line] : "<end of file>";
        if (want != got)
            return fmt::format("line {}\n- {}\n+ {}", line + 1, want, got);
    }
    return "";
}

// __FILE__ is relative to the repository root, the tests run from the build tree below it
std::filesystem::path golden_dir(const char *name)
{
    const std::filesystem::path relative = std::filesystem::path(__FILE__).parent_path() / "golden";
    for (std::filesystem::path dir = std::filesystem::current_path(); dir != dir.parent_path(); dir = dir.parent_path())
    {
        if (fs::is_dir((dir / relative).string()))
            return dir / relative / name;
    }
    return relative / name;
}

std::string golden_path(const char *name)
{
    return (golden_dir(golden_set) / (std::string(name) + ".txt")).string();
}

std::string actual_path(const char *name)
{
    return (std::filesystem::temp_directory_path() / "sihd_imgui_golden" / golden_set / (std::string(name) + ".txt"))
        .string();
}

std::string demo_test_name(const ::testing::TestParamInfo<DemoCase> & info)
{
    return info.param.name;
}

} // namespace

class TestNcursesDemo: public NcursesScene,
                       public ::testing::WithParamInterface<DemoCase>
{
    protected:
        TestNcursesDemo():
            NcursesScene(demo_width, GetParam().menu == nullptr ? GetParam().rows : window_height, cell_units)
        {
        }

        // the demo keeps the open example and tool windows in statics that outlive the context: close it again
        void TearDown() override
        {
            if (this->demo_case().menu != nullptr && !IsSkipped())
                this->open_from_menu();
        }

        const DemoCase & demo_case() const { return GetParam(); }

        ImVec2 units(float x, float y) const { return ImVec2(x * cell_size.x, y * cell_size.y); }

        void show_section() const
        {
            ImGui::LogToBuffer(log_open_depth);
            ImGui::ShowDemoWindow();
            ImGui::LogFinish();
            ImGuiWindow *demo = ImGui::FindWindowByName(demo_window);
            demo->StateStorage.SetInt(ImHashStr(this->demo_case().label, 0, demo->ID), 1);
            ImGui::SetWindowPos(demo_window, ImVec2(0, 0));
            ImGui::SetWindowSize(demo_window, this->units((float)width, (float)height));
        }

        // focusing every frame would close the menu popups
        void show_demo_in_front(bool focus = false) const
        {
            ImGui::ShowDemoWindow();
            ImGui::SetWindowCollapsed(demo_window, false);
            if (focus)
                ImGui::SetWindowFocus(demo_window);
            ImGui::SetWindowPos(demo_window, ImVec2(0, 0));
            ImGui::SetWindowSize(demo_window, this->units((float)width, (float)height));
        }

        // the opened windows share the screen in horizontal bands, the demo window collapsed under them
        void show_opened_windows() const
        {
            ImGui::ShowDemoWindow();
            ImGui::SetWindowCollapsed(demo_window, true);
            ImGui::SetWindowPos(demo_window, this->units(0, (float)height - 1));
            std::vector<ImGuiWindow *> opened;
            for (ImGuiWindow *window : GImGui->Windows)
            {
                const bool overlay = (window->Flags & (ImGuiWindowFlags_Popup | ImGuiWindowFlags_Tooltip)) != 0;
                if (window->Active && window->ParentWindow == nullptr && !overlay && !window->IsFallbackWindow
                    && std::string_view(window->Name) != demo_window)
                    opened.push_back(window);
            }
            const float band = std::floor((float)height / (float)std::max<size_t>(opened.size(), 1));
            for (size_t i = 0; i < opened.size(); ++i)
            {
                ImGui::SetWindowPos(opened[i], this->units(0, band * (float)i));
                ImGui::SetWindowSize(opened[i], this->units((float)width, band));
            }
        }

        std::optional<ImVec2> find_word(std::string_view word, int first_row) const
        {
            for (int y = first_row; y < height; ++y)
            {
                for (int x = 0; x + (int)word.size() <= width; ++x)
                {
                    bool match = (x == 0 || !is_word_char(cell(x - 1, y).ch))
                                 && (x + (int)word.size() == width || !is_word_char(cell(x + (int)word.size(), y).ch));
                    for (size_t k = 0; k < word.size() && match; ++k)
                        match = cell(x + (int)k, y).ch == (uint32_t)(unsigned char)word[k];
                    if (match)
                        return this->units((float)x + 0.5f, (float)y + 0.5f);
                }
            }
            return std::nullopt;
        }

        void click_word(std::string_view word, int first_row)
        {
            const std::optional<ImVec2> position = this->find_word(word, first_row);
            ASSERT_TRUE(position.has_value()) << "'" << word << "' not on screen:\n" << this->screen_text();
            ImGuiIO & io = ImGui::GetIO();
            io.AddMousePosEvent(position->x, position->y);
            // an event queued during a frame lands on the next one: hover, press and release on three frames
            int frame = 0;
            this->render_frames(3, [&] {
                this->show_demo_in_front();
                if (frame < 2)
                    io.AddMouseButtonEvent(ImGuiMouseButton_Left, frame == 0);
                ++frame;
            });
        }

        // toggles the menu item: the first call opens the window, the second closes it
        void open_from_menu()
        {
            bool focus = true;
            this->render_frames(2, [&] { this->show_demo_in_front(std::exchange(focus, false)); });
            // row 0 is the demo title bar, row 1 its menu bar
            this->click_word(this->demo_case().menu, 1);
            this->click_word(this->demo_case().label, 2);
            ImGui::GetIO().AddMousePosEvent(-FLT_MAX, -FLT_MAX);
        }

        std::string dump() const
        {
            std::vector<std::string> rows;
            for (int y = 0; y < height; ++y)
            {
                const std::string text = this->row_text(y);
                if (contains_pointer(text))
                    rows.emplace_back("-------- <row showing a pointer>");
                else
                    rows.push_back(fmt::format("{:08x} {}", row_style_hash(renderer, width, y), text));
            }
            // a row starts with its 8-digit style hash and a space
            while (rows.size() > 1 && is_empty_row(rows.back().substr(9)))
                rows.pop_back();
            return str::join(rows, "\n") + "\n";
        }
};

TEST_P(TestNcursesDemo, screen_matches_its_golden_file)
{
    if (GTEST_FLAG_GET(repeat) != 1)
        GTEST_SKIP() << "the demo animates from statics that outlive the ImGui context: only a first run starts fresh";
    if constexpr (!build::is_gnu)
        GTEST_SKIP() << "the golden files hold glibc's wcwidth, which other libcs differ from";

    const DemoCase & demo = this->demo_case();
    if (demo.menu == nullptr)
    {
        this->render_frames(settle_frames, [this] { this->show_section(); });
    }
    else
    {
        this->open_from_menu();
        this->render_frames(settle_frames, [this] { this->show_opened_windows(); });
    }
    const std::string actual = this->dump();
    const std::string golden = golden_path(demo.name);
    const std::string written = actual_path(demo.name);

    if (env::get(update_variable).has_value())
    {
        ASSERT_TRUE(fs::make_directories(std::filesystem::path(golden).parent_path().string()).has_value());
        ASSERT_TRUE(fs::write(golden, actual).has_value());
        return;
    }
    const Result<std::string> expected = fs::read_all(golden);
    ASSERT_TRUE(expected.has_value()) << golden << " missing: run with " << update_variable << "=1";
    const std::string difference = *expected == actual ? "" : first_difference(*expected, actual);
    if (!difference.empty())
    {
        EXPECT_TRUE(fs::make_directories(std::filesystem::path(written).parent_path().string()).has_value());
        EXPECT_TRUE(fs::write(written, actual).has_value());
    }

    EXPECT_EQ(difference, "") << "screen written to " << written << ", accept it with " << update_variable << "=1";
}

INSTANTIATE_TEST_SUITE_P(DemoWindows, TestNcursesDemo, ::testing::ValuesIn(demo_cases), demo_test_name);

} // namespace test

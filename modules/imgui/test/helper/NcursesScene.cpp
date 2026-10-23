#include "NcursesScene.hpp"

#include <imgui_internal.h>

#include <algorithm>
#include <array>
#include <vector>

#include <sihd/util/Logger.hpp>
#include <sihd/util/str.hpp>

namespace test
{

using namespace sihd::imgui;
using namespace sihd::util;

namespace
{

// the texture requests the renderer answers on each rasterize, for a frame left unrasterized
void ack_textures(const ImDrawData & draw_data)
{
    if (draw_data.Textures == nullptr)
        return;
    for (ImTextureData *tex : *draw_data.Textures)
    {
        if (tex->Status == ImTextureStatus_WantCreate)
        {
            tex->SetTexID((ImTextureID)1);
            tex->SetStatus(ImTextureStatus_OK);
        }
        else if (tex->Status == ImTextureStatus_WantUpdates)
        {
            tex->SetStatus(ImTextureStatus_OK);
        }
        else if (tex->Status == ImTextureStatus_WantDestroy)
        {
            tex->SetTexID(ImTextureID_Invalid);
            tex->SetStatus(ImTextureStatus_Destroyed);
        }
    }
}

} // namespace

NcursesScene::NcursesScene(int width, int height, ImVec2 cell_size):
    width(width),
    height(height),
    cell_size(cell_size),
    clear_color(0, 0, 0, 1)
{
    LoggerManager::stream();
    ctx = ImGui::CreateContext();
    ImGuiIO & io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)width * cell_size.x, (float)height * cell_size.y);
    io.DeltaTime = 1.0f / 60.0f;
    io.IniFilename = nullptr;
    io.ConfigInputTextCursorBlink = false;
    io.ConfigInputTrickleEventQueue = false;
    ImGui::StyleColorsDark();
    renderer.set_clear_color(&clear_color);
    renderer.set_cell_size(cell_size);
    renderer.init_headless(width, height);
}

NcursesScene::~NcursesScene()
{
    renderer.shutdown();
    ImGui::DestroyContext(ctx);
    LoggerManager::clear_loggers();
}

void NcursesScene::render_frames(int count, const std::function<void()> & build)
{
    for (int frame = 0; frame < count; ++frame)
    {
        renderer.new_frame();
        ImGui::NewFrame();
        build();
        ImGui::Render();
        // only the last frame is ever read: rasterizing the others costs a full grid each
        if (frame == count - 1)
            renderer.rasterize_headless(ImGui::GetDrawData());
        else
            ack_textures(*ImGui::GetDrawData());
    }
}

void NcursesScene::render_scene(const std::function<void()> & content)
{
    this->render_frames(2, [&] { this->scene_window(content); });
}

void NcursesScene::scene_window(const std::function<void()> & content) const
{
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)width, (float)height));
    ImGui::Begin("scene",
                 nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
                     | ImGuiWindowFlags_NoSavedSettings);
    content();
    ImGui::End();
}

const TCell & NcursesScene::cell(int x, int y) const
{
    return renderer.get_cell(x, y);
}

std::string NcursesScene::row_text(int y) const
{
    std::string row;
    for (int x = 0; x < width; ++x)
    {
        const TCell & current = renderer.get_cell(x, y);
        if (current.wide_tail)
            continue;
        std::array<char, 5> utf8 {};
        const int length = ImTextCharToUtf8(utf8.data(), current.ch == 0 ? (unsigned int)' ' : current.ch);
        row.append(utf8.data(), (size_t)length);
    }
    return std::string(str::rtrim(row));
}

std::string NcursesScene::screen_text() const
{
    std::string screen;
    for (int y = 0; y < height; ++y)
        screen += this->row_text(y) + "\n";
    return std::string(str::rtrim(screen));
}

std::string NcursesScene::row_backgrounds(int y) const
{
    std::vector<uint32_t> legend;
    std::string row;
    for (int x = 0; x < width; ++x)
    {
        const TCell & current = renderer.get_cell(x, y);
        if (!current.has_bg)
        {
            row += ' ';
            continue;
        }
        if (std::ranges::find(legend, current.bg) == legend.end())
            legend.push_back(current.bg);
        row += (char)('a' + (std::ranges::find(legend, current.bg) - legend.begin()));
    }
    return row;
}

int NcursesScene::first_col_with_text(int y, const std::string & text) const
{
    for (int x = 0; x + (int)text.size() <= width; ++x)
    {
        bool match = true;
        for (size_t k = 0; k < text.size() && match; ++k)
            match = renderer.get_cell(x + (int)k, y).ch == (uint32_t)(unsigned char)text[k];
        if (match)
            return x;
    }
    return -1;
}

} // namespace test

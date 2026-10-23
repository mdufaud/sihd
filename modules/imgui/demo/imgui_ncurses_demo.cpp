#include <clocale>

#include <sihd/imgui/ImguiBackendNcurses.hpp>
#include <sihd/imgui/ImguiRendererNcurses.hpp>
#include <sihd/imgui/ImguiRunner.hpp>
#include <sihd/sys/App.hpp>
#include <sihd/util/CliApp.hpp>
#include <sihd/util/Logger.hpp>

using namespace sihd::util;
using namespace sihd::imgui;

int main(int argc, char **argv)
{
    std::setlocale(LC_ALL, "");

    sihd::sys::App app({
        .name = "imgui_ncurses_demo",
        .description = "Imgui ncurses demo",
    });

    app.root().on_run([&] {
        ImguiRunner imgui("imgui-runner");
        if (!imgui.init_imgui())
            app.exit(EXIT_FAILURE);
        ImGuiIO & io = ImGui::GetIO();

        io.IniFilename = NULL;

        ImguiRendererNcurses ncurses_renderer;
        ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
        ncurses_renderer.set_clear_color(&clear_color);

        ImguiBackendNcurses ncurses_backend;
        if (!ncurses_backend.init())
            app.exit(EXIT_FAILURE);

        if (!ncurses_renderer.init())
            app.exit(EXIT_FAILURE);

        constexpr int image_size = 48;
        ImTextureData image;
        image.Create(ImTextureFormat_RGBA32, image_size, image_size);
        for (int y = 0; y < image_size; ++y)
        {
            for (int x = 0; x < image_size; ++x)
            {
                float red = 0.0f;
                float green = 0.0f;
                float blue = 0.0f;
                ImGui::ColorConvertHSVtoRGB((float)x / image_size,
                                            1.0f,
                                            1.0f - (float)y / image_size,
                                            red,
                                            green,
                                            blue);
                unsigned char *texel = image.Pixels + (y * image_size + x) * 4;
                texel[0] = (unsigned char)(red * 255.0f);
                texel[1] = (unsigned char)(green * 255.0f);
                texel[2] = (unsigned char)(blue * 255.0f);
                texel[3] = 255;
            }
        }

        bool show_demo_window = true;
        bool show_another_window = false;
        bool show_image_window = false;
        bool demo_window_placed = false;
        float slider_value = 0.0f;
        int click_count = 0;

        imgui.set_backend(&ncurses_backend);
        imgui.set_renderer(&ncurses_renderer);

        imgui.set_build_frame([&]() -> bool {
            if (show_demo_window)
                ImGui::ShowDemoWindow(&show_demo_window);

            // windows place themselves in pixels (demo at 650,20 sized 550x680, others at 60,60): one unit is a cell here
            if (!demo_window_placed)
            {
                const ImVec2 display = io.DisplaySize;
                ImGui::SetWindowPos("Dear ImGui Demo", ImVec2(display.x * 0.4f, 0.0f));
                ImGui::SetWindowSize("Dear ImGui Demo", ImVec2(display.x * 0.6f, display.y));
                demo_window_placed = true;
            }

            {
                ImGui::SetNextWindowPos(ImVec2(1.0f, 1.0f), ImGuiCond_FirstUseEver);
                ImGui::Begin("Hello, world!");

                ImGui::Text("This is some useful text.");
                ImGui::Checkbox("Demo Window", &show_demo_window);
                ImGui::Checkbox("Another Window", &show_another_window);
                ImGui::Checkbox("Image Window", &show_image_window);

                ImGui::SliderFloat("float", &slider_value, 0.0f, 1.0f);
                ImGui::ColorEdit3("clear color", (float *)&clear_color);

                if (ImGui::Button("Button"))
                    click_count++;
                ImGui::SameLine();
                ImGui::Text("counter = %d", click_count);

                ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                            1000.0f / ImGui::GetIO().Framerate,
                            ImGui::GetIO().Framerate);
                ImGui::End();
            }

            if (show_another_window)
            {
                ImGui::SetNextWindowPos(ImVec2(1.0f, 12.0f), ImGuiCond_FirstUseEver);
                ImGui::Begin("Another Window", &show_another_window);
                ImGui::Text("Hello from another window!");
                if (ImGui::Button("Close Me"))
                    show_another_window = false;
                ImGui::End();
            }

            if (show_image_window)
            {
                ImGui::SetNextWindowPos(ImVec2(1.0f, 12.0f), ImGuiCond_FirstUseEver);
                ImGui::Begin("Image Window", &show_image_window, ImGuiWindowFlags_AlwaysAutoResize);
                // a cell shows two pixels stacked: half as many rows as columns keeps the image square
                ImGui::Image(image.GetTexRef(), ImVec2(image_size, image_size / 2));
                ImGui::End();
            }
            return true;
        });
        imgui.run();
    });

    return app.run(argc, argv);
}

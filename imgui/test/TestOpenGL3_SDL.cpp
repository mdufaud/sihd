#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/term.hpp>

#include <sihd/imgui/ImguiBackendSDL.hpp>
#include <sihd/imgui/ImguiRendererOpenGL.hpp>
#include <sihd/imgui/ImguiRunner.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::imgui;
using namespace sihd::util;
class TestOpenGL3_SDL: public ::testing::Test
{
    protected:
        TestOpenGL3_SDL() { sihd::util::LoggerManager::stream(); }

        virtual ~TestOpenGL3_SDL() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}

        std::string _cwd;
        std::string _base_test_dir;
};

TEST_F(TestOpenGL3_SDL, test_opengl3_sdl_interactive)
{
    if (sihd::util::term::is_interactive() == false)
        GTEST_SKIP_("requires interaction");
    ImguiRunner imgui("imgui-runner");
    ASSERT_TRUE(imgui.init_imgui());

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    ImguiRendererOpenGL opengl_renderer;
    opengl_renderer.set_clear_color(&clear_color);

    ImguiBackendSDL sdl_backend;
    sdl_backend.decide_opengl_version();
    ASSERT_TRUE(sdl_backend.init_window("OpenGL3 SDL test"));
    ASSERT_TRUE(opengl_renderer.init());
    ASSERT_TRUE(sdl_backend.init_backend_opengl());

    bool show_demo_window = true;
    bool show_another_window = false;

    imgui.set_backend(&sdl_backend);
    imgui.set_renderer(&opengl_renderer);
    imgui.set_build_frame([&]() -> bool {
        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse
        // its code to learn more about Dear ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named
        // window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!"); // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text."); // Display some text (you can use a format strings too)
            ImGui::Checkbox("Demo Window",
                            &show_demo_window); // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &show_another_window);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f); // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float *)&clear_color); // Edit 3 floats representing a color

            if (ImGui::Button("Button")) // Buttons return true when clicked (most widgets return true when
                                         // edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                        1000.0f / ImGui::GetIO().Framerate,
                        ImGui::GetIO().Framerate);
            ImGui::End();
        }

        // 3. Show another simple window.
        if (show_another_window)
        {
            ImGui::Begin("Another Window",
                         &show_another_window); // Pass a pointer to our bool variable (the window will have a
                                                // closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }
        return true;
    });
    imgui.run();
}
} // namespace test
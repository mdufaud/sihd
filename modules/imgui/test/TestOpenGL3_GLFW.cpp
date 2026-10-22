#include <iostream>

#include <gtest/gtest.h>

#include <sihd/imgui/ImguiBackendGlfw.hpp>
#include <sihd/imgui/ImguiRendererOpenGL.hpp>
#include <sihd/imgui/ImguiRunner.hpp>
#include <sihd/sys/console.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::imgui;
using namespace sihd::util;
class TestOpenGL3_GLFW: public ::testing::Test
{
    protected:
        TestOpenGL3_GLFW() { LoggerManager::stream(); }

        virtual ~TestOpenGL3_GLFW() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}

        std::string _cwd;
        std::string _base_test_dir;
};

TEST_F(TestOpenGL3_GLFW, test_simple_interactive)
{
    if (sihd::sys::console::is_interactive() == false)
        GTEST_SKIP_("requires interaction");

    ImguiRunner imgui("imgui-runner");
    ASSERT_TRUE(imgui.init_imgui());

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    ImguiRendererOpenGL opengl_renderer;
    opengl_renderer.set_clear_color(&clear_color);

    ImguiBackendGlfw glfw_backend;
    ASSERT_TRUE(glfw_backend.init_window("OpenGL3 GLFW test"));
    ASSERT_TRUE(glfw_backend.init_backend_opengl());
    ASSERT_TRUE(opengl_renderer.init());

    bool show_demo_window = true;
    bool show_another_window = false;
    float slider_value = 0.0f;
    int click_count = 0;

    imgui.set_backend(&glfw_backend);
    imgui.set_renderer(&opengl_renderer);
    imgui.set_build_frame([&]() -> bool {
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        {
            ImGui::Begin("Hello, world!");

            ImGui::Text("This is some useful text.");
            ImGui::Checkbox("Demo Window", &show_demo_window);
            ImGui::Checkbox("Another Window", &show_another_window);

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
            ImGui::Begin("Another Window", &show_another_window);
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
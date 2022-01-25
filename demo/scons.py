import os

Import('env')

builder_helper = env["BUILDER_HELPER"]

demo_etc_dir = Dir("etc").Dir("sihd").Dir("demo")

if env["CXX"] == "em++":
    emscripten_env = env.Clone()
    emscripten_env.Replace(
        CPPFLAGS = [
            "-DIMGUI_DISABLE_FILE_FUNCTIONS",
            "-s", "USE_SDL=2",
        ],
        LINKFLAGS = [
            "--shell-file", str(demo_etc_dir.Dir("imgui_emscripten_sdl_demo").File("shell_minimal.html")),
            "-s", "WASM=1",
            "-s", "NO_FILESYSTEM=1",
            "-s", "ALLOW_MEMORY_GROWTH=1",
            "-s", "NO_EXIT_RUNTIME=0",
            "-s", "ASSERTIONS=1",
        ],
    )
    emscripten_env.build_bin(["src/imgui_emscripten_sdl_demo.cpp"], bin_name = "imgui_emscripten_sdl_demo.html")
    Return()

env.build_bin("src/http_demo.cpp", bin_name = "http_demo")
env.build_bin("src/pcap_demo.cpp", bin_name = "pcap_demo")
env.build_bin("src/imgui_opengl3_glfw_demo.cpp", bin_name = "imgui_opengl3_glfw_demo")
if builder_helper.build_platform == "windows":
    env.build_bin("src/imgui_win_d11_demo.cpp", bin_name = "imgui_win_d11_demo")
    env.build_bin("src/imgui_win_d11_sdl_demo.cpp", bin_name = "imgui_win_d11_sdl_demo")

Return()
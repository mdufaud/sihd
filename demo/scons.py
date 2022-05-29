import os

Import('env')

builder_helper = env["BUILDER_HELPER"]

demo_etc_dir = Dir("etc").Dir("sihd").Dir("demo")

if env["CXX"] == "em++":
    # env.Append(CPPFLAGS = ["-s", "WASM=1"])
    # env.build_bin(["src/emscripten_hello_world.cpp"], bin_name = "hello_world.html")
    emscripten_imgui_env = env.Clone()
    emscripten_imgui_env.Replace(
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
            "-s", "USE_SDL=2",
        ],
    )
    emscripten_imgui_env.build_bin(["src/imgui_emscripten_sdl_demo.cpp"], bin_name = "imgui_emscripten_sdl_demo.html")

    Return()

compile_sdl = os.getenv("sdl", None) == "1"

env.build_bin("src/pinger_demo.cpp", bin_name = "ping_demo")
env.build_bin("src/http_demo.cpp", bin_name = "http_demo")
env.build_bin("src/pcap_demo.cpp", bin_name = "pcap_demo")
env.build_bin("src/imgui_opengl3_glfw_demo.cpp", bin_name = "imgui_opengl3_glfw_demo")
if builder_helper.build_platform == "windows":
    env.build_bin("src/imgui_win_d11_demo.cpp", bin_name = "imgui_win_d11_demo")
    if compile_sdl:
        env.build_bin("src/imgui_win_d11_sdl_demo.cpp", bin_name = "imgui_win_d11_sdl_demo")

Return()
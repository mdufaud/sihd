import os

Import('env')

builder = env.builder()

demo_etc_dir = Dir("etc").Dir("sihd").Dir("demo")

if builder.build_compiler == "em":
    builder.warning("To build demo with emscripten you need to get static external libraries")
    builder.warning("To get external libraries, use command: make dep mod demo compiler=em")

    # must create specific env with just util as dependencies
    emscripten_util_env = env.get_depends_env(depends = ["util"])
    emscripten_util_env.Append(CPPFLAGS = ["-s", "WASM=1"])
    emscripten_util_env.build_bin(["src/emscripten_hello_world.cpp"], name = "hello_world.html")

    # must create specific env with just util and imgui as dependencies as it does not compile with pcap/http
    emscripten_imgui_env = env.get_depends_env(depends = ["util", "imgui"])
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
    emscripten_imgui_env.build_bin(["src/imgui_emscripten_sdl_demo.cpp"], name = "imgui_emscripten_sdl_demo.html")
else:
    compile_sdl = builder.get_opt("sdl", "") == "1"

    env.build_demo("src/imgui_opengl3_glfw_demo.cpp", name = "imgui_opengl3_glfw_demo")
    if builder.build_platform == "windows":
        env.build_demo("src/imgui_win_d11_demo.cpp", name = "imgui_win_d11_demo")
        if compile_sdl:
            env.build_demo("src/imgui_win_d11_sdl_demo.cpp", name = "imgui_win_d11_sdl_demo")
Return()
Import('env')

builder = env.builder()

# ncurses backend is a linux TUI: wide-char ncursesw comes from the vcpkg
# overlay-port (versioned), built from source for native and cross alike;
# headers live under extlib/include/ncursesw/ which is already in CPPPATH
build_ncurses = builder.build_platform == "linux"

# choose sources to build sihd_imgui lib

sihd_imgui_srcs = ["src/ImguiRunner.cpp"]
sihd_imgui_tests = [
    "test/main.cpp",
    "test/TestOpenGL3_GLFW.cpp",
    "test/TestOpenGL3_SDL.cpp",
]

if builder.build_platform == "android":
    ## Android: NativeActivity + EGL + OpenGL ES 3
    sihd_imgui_srcs.extend([
        "src/ImguiBackendAndroid.cpp",
        "src/ImguiRendererOpenGL.cpp",
    ])
    sihd_imgui_tests = [
        "test/main.cpp",
    ]
else:
    if builder.build_platform == "windows":
        ## Windows directX
        sihd_imgui_srcs.extend([
            "src/ImguiBackendWin32.cpp",
            "src/ImguiRendererDirectX.cpp",
        ])

    ## OpenGL renderer
    sihd_imgui_srcs.extend([
        "src/ImguiRendererOpenGL.cpp",
    ])

    ## Glfw (not available on web/emscripten)
    if builder.build_platform != "web":
        sihd_imgui_srcs.extend([
            "src/ImguiBackendGlfw.cpp",
        ])

    # SDL
    sihd_imgui_srcs.extend([
        "src/ImguiBackendSDL.cpp",
    ])

    if build_ncurses:
        sihd_imgui_srcs.extend([
            "src/ImguiBackendNcurses.cpp",
            "src/ImguiRendererNcurses.cpp",
        ])
        sihd_imgui_tests.append("test/TestNcursesRenderer.cpp")

# Lib

lib = env.build_lib(sihd_imgui_srcs, static_libs=["imgui"])

# Demos

if builder.build_platform == "android":
    env.build_demo("demo/imgui_opengl3_android_demo.cpp", name = "imgui_opengl3_android_demo", libs = [env.module_format_name()], android_dir = "android")
elif builder.build_platform == "web":
    env.build_demo("demo/imgui_opengl3_sdl_demo.cpp", name = "imgui_opengl3_sdl_demo", libs = [env.module_format_name()])
    demo_etc_dir = Dir("demo").Dir("etc").Dir("sihd").Dir("demo")
    env.Append(
        LINKFLAGS = [
            "--shell-file", str(demo_etc_dir.Dir("imgui_demo").File("shell_minimal.html")),
        ]
    )
else:
    env.build_demo("demo/imgui_opengl3_sdl_demo.cpp", name = "imgui_opengl3_sdl_demo", libs = [env.module_format_name()])
    env.build_demo("demo/imgui_opengl3_glfw_demo.cpp", name = "imgui_opengl3_glfw_demo", libs = [env.module_format_name()])

    if build_ncurses:
        env.build_demo("demo/imgui_ncurses_demo.cpp", name = "imgui_ncurses_demo", libs = [env.module_format_name()])

    if builder.build_platform == "windows":
        env.build_demo("demo/imgui_win_d11_demo.cpp", name = "imgui_win_d11_demo", libs = [env.module_format_name()])
        env.build_demo("demo/imgui_win_d11_sdl_demo.cpp", name = "imgui_win_d11_sdl_demo", libs = [env.module_format_name()])

# Tests

test = env.build_test(sihd_imgui_tests, libs = [env.module_format_name()])

Return('lib')

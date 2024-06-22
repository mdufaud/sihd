Import('env')

builder = env.builder()

conf = env["APP_MODULE_CONF"]

# choose sources to build sihd_imgui lib

sihd_imgui_srcs = ["src/ImguiRunner.cpp"]
sihd_imgui_tests = [
    "test/main.cpp",
    "test/TestCompilation.cpp",
    "test/TestOpenGL3_GLFW.cpp"
]

## Windows directX

if builder.build_platform == "windows":
    sihd_imgui_srcs.extend([
        "src/ImguiBackendWin32.cpp",
        "src/ImguiRendererDirectX.cpp",
    ])

## Glfw + OpenGL

sihd_imgui_srcs.extend([
    "src/ImguiBackendGlfw.cpp",
    "src/ImguiRendererOpenGL.cpp",
])

## Check for SDL2
compile_sdl = builder.is_opt("sdl")
compiling_with_emscripten = builder.build_compiler == "em"

env.Prepend(LIBS = "imgui")
lib = env.build_lib(sihd_imgui_srcs)

env.build_demo("demo/imgui_opengl3_glfw_demo.cpp", name = "imgui_opengl3_glfw_demo", add_libs = [env.module_format_name()])
if builder.build_platform == "windows":
    env.build_demo("demo/imgui_win_d11_demo.cpp", name = "imgui_win_d11_demo", add_libs = [env.module_format_name()])
    if compile_sdl:
        env.build_demo("demo/imgui_win_d11_sdl_demo.cpp", name = "imgui_win_d11_sdl_demo", add_libs = [env.module_format_name()])

# test
test = env.build_test(sihd_imgui_tests, add_libs = [env.module_format_name()])

Return('lib')

Import('env')

builder = env.builder()

conf = env["APP_MODULE_CONF"]

## Clone ImGui repository

# env.git_clone(conf["git-url"], conf["git-branch"], "imgui")

## Build ImGui and sihd_imgui libraries

# imgui_dir = Dir("imgui")
# imgui_backends_dir = imgui_dir.Dir("backends")

# # choose sources to build imgui lib
# imgui_srcs = Glob(str(imgui_dir) + "/*.cpp")
# imgui_headers = Glob(str(imgui_dir) + "/*.h")

# choose sources to build sihd_imgui lib

sihd_imgui_srcs = ["src/ImguiRunner.cpp"]
sihd_imgui_tests = [
    "test/main.cpp",
    "test/TestCompilation.cpp",
    "test/TestOpenGL3_GLFW.cpp"
]

# getting supported backends
# imgui_headers.extend([
#     imgui_backends_dir.File("imgui_impl_glfw.h"),
#     imgui_backends_dir.File("imgui_impl_opengl3.h"),
#     imgui_backends_dir.File("imgui_impl_opengl3_loader.h"),
#     imgui_backends_dir.File("imgui_impl_sdl.h"),
# ])

## Windows directX

if builder.build_platform == "windows":
    # imgui_srcs.extend([
    #     imgui_backends_dir.File("imgui_impl_dx11.cpp"),
    #     imgui_backends_dir.File("imgui_impl_win32.cpp"),
    # ])
    # imgui_headers.extend([
    #     imgui_backends_dir.File("imgui_impl_dx11.h"),
    #     imgui_backends_dir.File("imgui_impl_win32.h"),
    # ])
    sihd_imgui_srcs.extend([
        "src/ImguiBackendWin32.cpp",
        "src/ImguiRendererDirectX.cpp",
    ])

## Glfw + OpenGL

# imgui_srcs.extend([
#     imgui_backends_dir.File("imgui_impl_glfw.cpp"),
#     imgui_backends_dir.File("imgui_impl_opengl3.cpp"),
# ])
sihd_imgui_srcs.extend([
    "src/ImguiBackendGlfw.cpp",
    "src/ImguiRendererOpenGL.cpp",
])

## Check for SDL2
compile_sdl = builder.is_opt("sdl")
compiling_with_emscripten = builder.build_compiler == "em"

# if compiling_with_emscripten:
#     env.Append(
#         LIBS = ["SDL2"],
#         CPPFLAGS = [
#             "-s", "USE_SDL=2",
#             "-s", "WASM=1",
#             "-s", "NO_FILESYSTEM=1",
#             "-s", "ALLOW_MEMORY_GROWTH=1",
#             "-s", "NO_EXIT_RUNTIME=0",
#             "-s", "ASSERTIONS=1",
#             "-s", "OFFSCREEN_FRAMEBUFFER=1"
#         ],
#     )
# elif compile_sdl:
#     # try loading lib config if installed on system
#     if not env.parse_config("sdl2-config --libs --cflags"):
#         # try appending SDL2 from extlib repository
#         extlib_sdl_dir = Dir(builder.build_extlib_hdr_path).Dir("SDL2")
#         env.Append(CPPPATH = [str(extlib_sdl_dir)])
#         env.Append(LIBS = ["SDL2"])

# append extra SDL sources
# if compile_sdl or compiling_with_emscripten:
#     imgui_srcs.append(imgui_backends_dir.File("imgui_impl_sdl.cpp"))
#     sihd_imgui_srcs.append("src/ImguiBackendSDL.cpp")
#     sihd_imgui_tests.append("test/TestOpenGL3_SDL.cpp")

## Copy ImGui headers into build

# build_include_dir = builder.build_hdr_path
# sihd_imgui_build_include_dir = "include/sihd/imgui"

# builder.info("imgui: copying headers to: " + str(sihd_imgui_build_include_dir))
# for header in imgui_headers:
#     env.copy_into_build(header, "include/sihd/imgui")

## Build libimgui and libsihd_imgui
# imgui_env = env.Clone()
## append path "imgui" and "imgui/backends"
# imgui_env.Append(CPPPATH = [str(imgui_dir), str(imgui_backends_dir)])
# imgui_env.Append(CPPFLAGS = "-Wno-misleading-indentation")

# imgui_lib = imgui_env.build_lib(imgui_srcs, name = "imgui")

env.Prepend(LIBS = "imgui")
lib = env.build_lib(sihd_imgui_srcs)

# demo
# if builder.build_compiler == "em":
#     demo_etc_dir = Dir("etc").Dir("sihd").Dir("demo")
#     env.Replace(
#         CPPFLAGS = [
#             "-DIMGUI_DISABLE_FILE_FUNCTIONS",
#             "-s", "USE_SDL=2",
#         ],
#         LINKFLAGS = [
#             "--shell-file", str(demo_etc_dir.Dir("imgui_emscripten_sdl_demo").File("shell_minimal.html")),
#             "-s", "WASM=1",
#             "-s", "NO_FILESYSTEM=1",
#             "-s", "ALLOW_MEMORY_GROWTH=1",
#             "-s", "NO_EXIT_RUNTIME=0",
#             "-s", "ASSERTIONS=1",
#             "-s", "USE_SDL=2",
#         ],
#     )
#     env.build_demo(["demo/imgui_emscripten_sdl_demo.cpp"], name = "imgui_emscripten_sdl_demo.html")
# else:
env.build_demo("demo/imgui_opengl3_glfw_demo.cpp", name = "imgui_opengl3_glfw_demo", add_libs = [env.module_format_name()])
if builder.build_platform == "windows":
    env.build_demo("demo/imgui_win_d11_demo.cpp", name = "imgui_win_d11_demo", add_libs = [env.module_format_name()])
    if compile_sdl:
        env.build_demo("demo/imgui_win_d11_sdl_demo.cpp", name = "imgui_win_d11_sdl_demo", add_libs = [env.module_format_name()])

# test
test = env.build_test(sihd_imgui_tests, add_libs = [env.module_format_name()])

Return('lib')

import os

Import('env')

builder_helper = env["BUILDER_HELPER"]
conf = env["APP_MODULE_CONF"]

## Check for SDL2
has_sdl2 = os.getenv("sdl") == "1"
if has_sdl2:
    extlib_sdl_dir = Dir(builder_helper.build_extlib_hdr_path).Dir("SDL2")
    env.Append(CPPPATH = [str(extlib_sdl_dir)])
    env.Append(LIBS = ["SDL2main", "SDL2"])

## Clone ImGui repository

imgui_dir = Dir("imgui")
if not os.path.isdir(str(imgui_dir)):
    builder_helper.info("imgui: cloning repository")
    import subprocess
    subprocess.call(['git', 'clone', '--branch', conf["git-branch"], conf["git-url"], str(imgui_dir)])
else:
    builder_helper.info("imgui: repository already cloned")

## Build ImGui and sihd_imgui libraries

# append path "imgui" and "imgui/backends"
imgui_backends_dir = imgui_dir.Dir("backends")
env.Append(CPPPATH = [str(imgui_dir), str(imgui_backends_dir)])

# choose sources to build imgui lib
imgui_srcs = Glob(str(imgui_dir) + "/*.cpp")
imgui_headers = Glob(str(imgui_dir) + "/*.h")

# choose sources to build sihd_imgui lib
build_include_dir = builder_helper.build_hdr_path
sihd_imgui_include_dir = Dir(build_include_dir).Dir("sihd").Dir("imgui")

sihd_imgui_srcs = ["src/ImguiRunner.cpp"]
sihd_imgui_tests = [
    "test/main.cpp",
    "test/TestCompilation.cpp",
    "test/TestOpenGL3_GLFW.cpp"
]

## Windows directX

if builder_helper.build_platform == "windows":
    imgui_srcs.extend([
        imgui_backends_dir.File("imgui_impl_dx11.cpp"),
        imgui_backends_dir.File("imgui_impl_win32.cpp"),
    ])
    imgui_headers.extend([
        imgui_backends_dir.File("imgui_impl_dx11.h"),
        imgui_backends_dir.File("imgui_impl_win32.h"),
    ])
    sihd_imgui_srcs.extend([
        "src/ImguiBackendWin32.cpp",
        "src/ImguiRendererDirectX.cpp",
    ])

## Glfw + OpenGL

imgui_srcs.extend([
    imgui_backends_dir.File("imgui_impl_glfw.cpp"),
    imgui_backends_dir.File("imgui_impl_opengl3.cpp"),
])
imgui_headers.extend([
    imgui_backends_dir.File("imgui_impl_glfw.h"),
    imgui_backends_dir.File("imgui_impl_opengl3.h"),
    imgui_backends_dir.File("imgui_impl_opengl3_loader.h"),
])
sihd_imgui_srcs.extend([
    "src/ImguiBackendGlfw.cpp",
    "src/ImguiRendererOpenGL.cpp",
])

# append extra if SDL2 found
if has_sdl2:
    imgui_srcs.append(imgui_backends_dir.File("imgui_impl_sdl.cpp"))
    imgui_headers.append(imgui_backends_dir.File("imgui_impl_sdl.h"))
    sihd_imgui_srcs.append("src/ImguiBackendSDL.cpp")
    sihd_imgui_tests.append("test/TestOpenGL3_SDL.cpp")

## Copy ImGui headers into build

import shutil
builder_helper.info("imgui: copying headers to: " + str(sihd_imgui_include_dir))
os.makedirs(str(sihd_imgui_include_dir), exist_ok = True)
for header in imgui_headers:
    shutil.copy(str(header), str(sihd_imgui_include_dir))

## Build libimgui and libsihd_imgui

imgui_lib = env.build_lib(imgui_srcs, lib_name = "imgui")
env.Prepend(LIBS = "imgui")
lib = env.build_lib(sihd_imgui_srcs, lib_name = env['APP_MODULE_FORMAT_NAME'])
test = env.build_test(sihd_imgui_tests, add_libs = [env['APP_MODULE_FORMAT_NAME']])

Return('lib')

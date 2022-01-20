Import('env')

builder_helper = env["BUILDER_HELPER"]
conf = env["APP_MODULE_CONF"]

# clone imgui if not already done
import os
imgui_dir = Dir("imgui")
if not os.path.isdir(str(imgui_dir)):
    builder_helper.info("imgui: cloning repository")
    import subprocess
    subprocess.call(['git', 'clone', '--branch', conf["git-branch"], conf["git-url"], str(imgui_dir)])
else:
    builder_helper.info("imgui: repository already cloned")

# append path
imgui_backends_dir = imgui_dir.Dir("backends")
env.Append(CPPPATH = [str(imgui_dir), str(imgui_backends_dir)])

# choose sources to build
imgui_srcs = Glob(str(imgui_dir) + "/*.cpp")
imgui_headers = Glob(str(imgui_dir) + "/*.h")
# platform dependent srcs/headers
platform = env["BUILDER_HELPER"].build_platform
if platform == "windows":
    imgui_srcs.extend([
        imgui_backends_dir.File("imgui_impl_dx11.cpp"),
        imgui_backends_dir.File("imgui_impl_win32.cpp"),
    ])
    imgui_headers.extend([
        imgui_backends_dir.File("imgui_impl_dx11.h"),
        imgui_backends_dir.File("imgui_impl_win32.h"),
    ])
else:
    imgui_srcs.extend([
        imgui_backends_dir.File("imgui_impl_glfw.cpp"),
        imgui_backends_dir.File("imgui_impl_opengl3.cpp"),
    ])
    imgui_headers.extend([
        imgui_backends_dir.File("imgui_impl_glfw.h"),
        imgui_backends_dir.File("imgui_impl_opengl3.h"),
    ])

# copy headers into build
import shutil
build_include_dir = env["APP_BUILD_INCLUDE"]
imgui_include_dir = os.path.join(str(build_include_dir), "sihd", "imgui")
builder_helper.info("imgui: copying headers to: " + imgui_include_dir)
os.makedirs(imgui_include_dir, exist_ok = True)
for header in imgui_headers:
    shutil.copy(str(header), imgui_include_dir)

# build
lib = env.build_lib(imgui_srcs, lib_name = "imgui")
env.Prepend(LIBS = "imgui")

test = env.build_test(add_libs = None)

if platform == "windows":
    env.build_bin(["bin/win_d11_demo.cpp"], bin_name = "imgui_win_d11_demo")

Return()

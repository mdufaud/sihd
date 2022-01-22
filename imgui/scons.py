import os

Import('env')

has_sdl2 = True
scons_log_file = File(".scons_conf.log")
scons_conf_dir = Dir(".scons_conf.d")
scons_conf = env.Configure(log_file = scons_log_file, conf_dir = scons_conf_dir)
if scons_conf.CheckLib('SDL2'):
    has_sdl2 = True

builder_helper = env["BUILDER_HELPER"]
conf = env["APP_MODULE_CONF"]

# clone imgui if not already done
imgui_dir = Dir("imgui")
if not os.path.isdir(str(imgui_dir)):
    builder_helper.info("imgui: cloning repository")
    import subprocess
    subprocess.call(['git', 'clone', '--branch', conf["git-branch"], conf["git-url"], str(imgui_dir)])
else:
    builder_helper.info("imgui: repository already cloned")

# append path "imgui" and "imgui/backends"
imgui_backends_dir = imgui_dir.Dir("backends")
env.Append(CPPPATH = [str(imgui_dir), str(imgui_backends_dir)])

# choose sources to build
imgui_srcs = Glob(str(imgui_dir) + "/*.cpp")
imgui_headers = Glob(str(imgui_dir) + "/*.h")
# platform dependent srcs/headers
if builder_helper.build_platform == "windows":
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
    if has_sdl2:
        imgui_srcs.append(imgui_backends_dir.File("imgui_impl_sdl.cpp"))
        imgui_headers.append(imgui_backends_dir.File("imgui_impl_sdl.h"))

# copy headers into build
import shutil
build_include_dir = builder_helper.build_hdr_path
imgui_include_dir = os.path.join(str(build_include_dir), "sihd", "imgui")
builder_helper.info("imgui: copying headers to: " + imgui_include_dir)
os.makedirs(imgui_include_dir, exist_ok = True)
for header in imgui_headers:
    shutil.copy(str(header), imgui_include_dir)

# build
imgui_lib = env.build_lib(imgui_srcs, lib_name = "imgui")
env.Prepend(LIBS = "imgui")

lib = env.build_lib(Glob('src/*.cpp'), lib_name = env['APP_MODULE_FORMAT_NAME'])
test = env.build_test(Glob('test/*.cpp'), add_libs = [env['APP_MODULE_FORMAT_NAME']])

Return('lib')

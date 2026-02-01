Import('env')

builder = env.builder()
conf = env.module_conf()

srcs = Glob('src/*.cpp')

lib = env.build_lib(srcs)

sihd_util_libname = env.module_format_name()

compile_x11 = env.is_opt("x11")
compile_wayland = env.is_opt("wayland")

if compile_x11:
    env.Append(LIBS = ["X11"])
    env.Append(CPPDEFINES = ["SIHD_COMPILE_WITH_X11"])

if compile_wayland:
    env.Append(LIBS = ["wayland-client"])
    env.Append(CPPDEFINES = ["SIHD_COMPILE_WITH_WAYLAND"])

env.build_demo("demo/util_demo.cpp", name = "util_demo", add_libs = [sihd_util_libname])

if builder.build_platform != "web":
    env.build_demo("demo/file_watcher.cpp", name = "file_watcher", add_libs = [sihd_util_libname])
    env.build_demo("demo/file_poller.cpp", name = "file_poller", add_libs = [sihd_util_libname])
    env.build_demo("demo/process_info.cpp", name = "process_info", add_libs = [sihd_util_libname])

test = env.build_test(Glob('test/*.cpp'), add_libs = [sihd_util_libname])
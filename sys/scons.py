Import('env')

builder = env.builder()

compile_x11 = env.is_opt("x11")
compile_wayland = env.is_opt("wayland")

if compile_x11:
    env.Append(LIBS = ["X11"])
    env.Append(CPPDEFINES = ["SIHD_COMPILE_WITH_X11"])

if compile_wayland:
    env.Append(LIBS = ["wayland-client"])
    env.Append(CPPDEFINES = ["SIHD_COMPILE_WITH_WAYLAND"])

sihd_sys_libname = env.module_format_name()

lib = env.build_lib(Glob('src/*.cpp'))

env.build_demo("demo/sys_demo.cpp", name = "sys_demo", add_libs = [sihd_sys_libname])

if builder.build_platform != "web":
    env.build_demo("demo/file_watcher.cpp", name = "file_watcher", add_libs = [sihd_sys_libname])
    env.build_demo("demo/file_poller.cpp", name = "file_poller", add_libs = [sihd_sys_libname])
    env.build_demo("demo/process_info.cpp", name = "process_info", add_libs = [sihd_sys_libname])

test = env.build_test(Glob('test/*.cpp'), add_libs = [sihd_sys_libname])

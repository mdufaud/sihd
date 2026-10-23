Import('env')

builder = env.builder()

compile_x11 = env.is_opt("x11")
compile_wayland = env.is_opt("wayland")

if compile_x11:
    env.Append(LIBS = ["X11", "Xtst", "Xrandr"])

if compile_wayland:
    env.Append(LIBS = ["wayland-client"])

sihd_sys_libname = env.module_format_name()

sources = Glob('src/*.cpp')
# platform folders hold the implementations with no common shape (the cap:: platform
# functions and CapabilitySet); the linux ones self-guard and compile as no-ops on android/web
if builder.build_platform == "windows":
    sources += Glob('src/windows/*.cpp')
    sources += Glob('src/windows/internal/*.cpp')
else:
    sources += Glob('src/linux/*.cpp')
    # shared plumbing for the clipboard / screenshot backends
    sources += Glob('src/linux/internal/*.cpp')
    # x11 / wayland backends compile when their opt is enabled, their stub otherwise: no define, the pch stays shared
    if compile_x11:
        sources += Glob('src/linux/x11/*.cpp')
    else:
        sources += ['src/linux/stub/x11.cpp']
    if compile_wayland:
        sources += Glob('src/linux/wayland/*.cpp')
        # vendored wayland protocol definitions (wayland-scanner private-code)
        sources += Glob('src/linux/wayland/protocols/*.c')
    else:
        sources += ['src/linux/stub/wayland.cpp']

lib = env.build_lib(sources)

env.build_demo("demo/sys_demo.cpp", name = "sys_demo", libs = [sihd_sys_libname])
env.build_demo("demo/app_demo.cpp", name = "app_demo", libs = [sihd_sys_libname])

if builder.build_platform != "web":
    env.build_demo("demo/file_watcher.cpp", name = "file_watcher", libs = [sihd_sys_libname])
    env.build_demo("demo/file_poller.cpp", name = "file_poller", libs = [sihd_sys_libname])
    env.build_demo("demo/process_info.cpp", name = "process_info", libs = [sihd_sys_libname])
    env.build_demo("demo/process_monitor.cpp", name = "process_monitor", libs = [sihd_sys_libname])
    env.build_demo("demo/fs_info.cpp", name = "fs_info", libs = [sihd_sys_libname])
    env.build_demo("demo/sys_info.cpp", name = "sys_info", libs = [sihd_sys_libname])

test_sources = Glob('test/*.cpp')
if builder.build_platform == "windows":
    test_sources += Glob('test/windows/*.cpp')
else:
    test_sources += Glob('test/linux/*.cpp')

test = env.build_test(test_sources, libs = [sihd_sys_libname])
env.build_bench(Glob('bench/*.cpp'), libs = [sihd_sys_libname])

if builder.build_tests:
    env.build_bin("test/helper/main.cpp", name = "sihd_sys_test_helper", libs = [sihd_sys_libname])
    if builder.build_platform == "windows":
        env.build_bin("test/windows/service/main.cpp", name = "sihd_sys_test_service", libs = [sihd_sys_libname])

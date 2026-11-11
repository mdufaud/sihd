import shutil
import subprocess

Import('env')

builder = env.builder()
sihd_usb_libname = env.module_format_name()

sources = Glob('src/*.cpp')
test_sources = Glob('test/*.cpp')
if builder.build_platform != "windows":
    sources += Glob('src/linux/*.cpp')
    test_sources += Glob('test/linux/*.cpp')

lib = env.build_lib(sources)

if builder.build_platform == "android":
    env.build_demo("demo/android/usb_control.cpp", name = "usb_control", libs = [sihd_usb_libname], android_dir = "android")
else:
    for src in Glob('demo/*.cpp'):
        env.build_demo(src, name = env.file_basename(src), libs = [sihd_usb_libname])


def umockdev_flags():
    if builder.build_platform != "linux" or builder.is_cross_building() or shutil.which("pkg-config") is None:
        return {}
    # include paths only: a -pthread from --cflags would break the shared pch
    query = subprocess.run(["pkg-config", "--cflags-only-I", "--libs", "umockdev-1.0"], capture_output = True, text = True)
    if query.returncode != 0:
        return {}
    flags = env.ParseFlags(query.stdout)
    return {key: flags[key] for key in ("CPPPATH", "LIBPATH", "LIBS")}


# test/linux/FakeUsbDevice.cpp serves a fake usb device under umockdev-wrapper when umockdev is installed
test_env = env.Clone()
test_env.Append(**umockdev_flags())
test = test_env.build_test(test_sources, libs = [sihd_usb_libname])

Return('lib')

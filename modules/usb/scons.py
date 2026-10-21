Import('env')

builder = env.builder()
sihd_usb_libname = env.module_format_name()

sources = Glob('src/*.cpp')
test_sources = Glob('test/*.cpp')
if builder.build_platform != "windows":
    sources += Glob('src/linux/*.cpp')
    test_sources += Glob('test/linux/*.cpp')

lib = env.build_lib(sources)

for src in Glob('demo/*.cpp'):
    env.build_demo(src, name = env.file_basename(src), libs = [sihd_usb_libname])

test = env.build_test(test_sources, libs = [sihd_usb_libname])

Return('lib')

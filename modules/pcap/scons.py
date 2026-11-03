Import('env')

lib = env.build_lib(Glob('src/*.cpp'))

if env.builder().build_platform == "android":
    env.build_demo("demo/android/vpn_capture.cpp", name = "vpn_capture", libs = [env.module_format_name()], android_dir = "android")
else:
    for src in Glob('demo/*.cpp'):
        name = env.file_basename(src)
        demo = env.build_demo(src, name = name, libs = [env.module_format_name()])

test_sources = Glob('test/*.cpp')
if env.builder().build_platform != "windows":
    test_sources += Glob('test/linux/*.cpp')

test = env.build_test(test_sources, libs = [env.module_format_name()])

for src in Glob('fuzz/*.cpp'):
    env.build_fuzz(src, name = env.module_format_name() + '_' + env.file_basename(src), libs = [env.module_format_name()])

Return('lib')

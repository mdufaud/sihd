Import('env')

builder = env.builder()

sources = Glob('src/*.cpp')
test_sources = Glob('test/*.cpp')
if builder.build_platform == "windows":
    sources += Glob('src/windows/*.cpp')
    test_sources += Glob('test/windows/*.cpp')
elif builder.build_platform == "android":
    sources += Glob('src/android/*.cpp')
else:
    sources += Glob('src/linux/*.cpp')
    test_sources += Glob('test/linux/*.cpp')

lib = env.build_lib(sources)

if builder.build_platform == "android":
    env.build_demo(["demo/bt_demo.cpp", "demo/android/bt_jni.cpp"], name = "bt_demo", libs = [env.module_format_name()], android_dir = "android")
else:
    for src in Glob('demo/*.cpp'):
        env.build_demo(src, name = env.file_basename(src), libs = [env.module_format_name()])

test = env.build_test(test_sources, libs = [env.module_format_name()])

Return('lib')

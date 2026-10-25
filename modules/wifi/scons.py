Import('env')

builder = env.builder()

sources = Glob('src/*.cpp')
test_sources = Glob('test/*.cpp')
if builder.build_platform == "windows":
    sources += Glob('src/windows/*.cpp')
    test_sources += Glob('test/windows/*.cpp')
else:
    sources += Glob('src/linux/*.cpp')
    test_sources += Glob('test/linux/*.cpp')

lib = env.build_lib(sources)

for src in Glob('demo/*.cpp'):
    env.build_demo(src, name = env.file_basename(src), libs = [env.module_format_name()])

test = env.build_test(test_sources, libs = [env.module_format_name()])

Return('lib')

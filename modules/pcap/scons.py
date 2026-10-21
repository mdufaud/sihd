Import('env')

lib = env.build_lib(Glob('src/*.cpp'))

for src in Glob('demo/*.cpp'):
    name = env.file_basename(src)
    demo = env.build_demo(src, name = name, libs = [env.module_format_name()])

test_sources = Glob('test/*.cpp')
if env.builder().build_platform != "windows":
    test_sources += Glob('test/linux/*.cpp')

test = env.build_test(test_sources, libs = [env.module_format_name()])

Return('lib')

Import('env')

builder = env.builder()

sources = Glob('src/*.cpp')
if builder.build_platform == "windows":
    sources += Glob('src/windows/*.cpp')
else:
    sources += Glob('src/linux/*.cpp')

lib = env.build_lib(sources)

for src in Glob('demo/*.cpp'):
    env.build_demo(src, name = env.file_basename(src), libs = [env.module_format_name()])

test = env.build_test(Glob('test/*.cpp'), libs = [env.module_format_name()])

Return('lib')

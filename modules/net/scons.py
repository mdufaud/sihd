Import('env')

builder = env.builder()

sources = Glob('src/*.cpp')
# platform folders hold the implementations that diverge per OS (Socket, NetInterface);
# the linux ones self-guard and compile on android/web/osx as well
if builder.build_platform == "windows":
    sources += Glob('src/windows/*.cpp')
else:
    sources += Glob('src/linux/*.cpp')

lib = env.build_lib(sources)

for src in Glob('demo/*.cpp'):
    name = env.file_basename(src)
    demo = env.build_demo(src, name = name, libs = [env.module_format_name()])

test = env.build_test(Glob('test/*.cpp'), libs = [env.module_format_name()])

Return('lib')

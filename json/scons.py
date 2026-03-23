Import('env')

lib = env.build_lib(Glob('src/*.cpp'))
test = env.build_test(Glob('test/*.cpp'), add_libs = [env.module_format_name()])

for src in Glob('demo/*.cpp'):
    env.build_demo(src, name = env.file_basename(src), add_libs = [env.module_format_name()])

Return('lib')

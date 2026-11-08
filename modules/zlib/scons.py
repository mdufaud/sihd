Import('env')

lib = env.build_lib(Glob('src/*.cpp'))
test = env.build_test(Glob('test/*.cpp'), libs = [env.module_format_name()])

for src in Glob('fuzz/*.cpp'):
    env.build_fuzz(src, name = env.module_format_name() + '_' + env.file_basename(src), libs = [env.module_format_name()])

Return('lib')

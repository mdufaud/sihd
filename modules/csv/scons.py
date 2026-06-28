Import('env')

lib = env.build_lib(Glob('src/*.cpp'))
test = env.build_test(Glob('test/*.cpp'), libs = [env.module_format_name()])

Return('lib')

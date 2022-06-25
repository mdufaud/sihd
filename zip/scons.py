Import('env')

lib = env.build_lib(Glob('src/*.cpp'))
test = env.build_test(Glob('test/*.cpp'), add_libs = [env['APP_MODULE_FORMAT_NAME']])

Return('lib')

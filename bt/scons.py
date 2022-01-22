Import('env')

lib = env.build_lib(Glob('src/*.cpp'), lib_name = env['APP_MODULE_FORMAT_NAME'])
test = env.build_test(Glob('test/*.cpp'), add_libs = [env['APP_MODULE_FORMAT_NAME']])

Return('lib')

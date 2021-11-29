Import('env')

lib = env.build_lib()
test = env.build_test()

Return('lib')

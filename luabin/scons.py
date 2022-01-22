Import('env')

bin = env.build_bin("src/main.cpp", add_libs = [env['APP_MODULE_FORMAT_NAME']])

Return("bin")
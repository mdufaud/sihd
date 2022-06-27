Import('env', 'builder_helper', 'module_format_name')

# build library from lib sources - not added to environnement
lib = env.build_lib(Glob('src/*.cpp'))

# build unittest from test sources with newly created lib
test = env.build_test(Glob('test/*.cpp'), add_libs = [env['APP_MODULE_FORMAT_NAME']])

# build binary from bin sources with newly created lib
# bin = env.build_bin(Glob('src/*.cpp'), add_libs = [env['APP_MODULE_FORMAT_NAME']])

binaries = []
binaries.append(env.build_bin(Glob('src/bin/rain.c'), name = "rain"))
binaries.append(env.build_bin(Glob('src/bin/test.c'), name = "test"))
binaries.append(env.build_bin(Glob('src/bin/worm.c'), name = "worm"))
binaries.append(env.build_bin(Glob('src/bin/xmas.c'), name = "xmas"))
binaries.append(env.build_bin(Glob('src/bin/sihd.cpp'), name = "sihd_curses", add_libs = [module_format_name]))

Return('binaries')

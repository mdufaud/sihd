Import('env')

builder = env.builder()

# build library from lib sources - not added to environnement
lib = env.build_lib(Glob('src/*.cpp'), name = env.module_format_name())

# build unittest from test sources with newly created lib
test = env.build_test(Glob('test/*.cpp'), add_libs = [env.module_format_name()])

# build binary from bin sources with newly created lib
# bin = env.build_bin(Glob('src/*.cpp'), add_libs = [env.module_format_name()])
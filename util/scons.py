Import('env')

src = Glob('src/*.cpp')
ext_src = Glob('ext_src/*.c')

lib = env.build_lib(src + ext_src)
test = env.build_test()

Return("lib")
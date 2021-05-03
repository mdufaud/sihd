Import('env')

prog = env.build_bin("src/main.cpp")
#test = env.build_test()

Return("prog")
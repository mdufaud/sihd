Import('env')

prog = env.build_bin("src/main.cpp")

Return("prog")
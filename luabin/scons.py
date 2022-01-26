Import('env')

bin = env.build_bin("src/main.cpp")

Return("bin")
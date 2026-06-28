Import('env')

bin = env.build_bin("src/main.cpp", name = "sihd_lua")

Return("bin")
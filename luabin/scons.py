Import('env')

bin = env.build_bin("src/main.cpp", bin_name = "sihd_lua")

Return("bin")
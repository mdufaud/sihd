Import('env')

builder_helper = env["BUILDER_HELPER"]
if env["CXX"] != "em++":
    builder_helper.error("emscripten: must be built using emscripten compiler (add compiler=em)")
    Return('')

env.Append(CPPFLAGS = ["-s", "WASM=1"])

bin = env.build_bin("src/main.cpp", bin_name = "hello.html")

Return('bin')
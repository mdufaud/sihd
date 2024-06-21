Import('env')

builder = env.builder()

# build library from lib sources - not added to environnement
lib = env.build_lib(Glob('src/*.cpp'), name = env.module_format_name())

env.build_demos(Glob("demo/*.cpp"), add_libs = [env.module_format_name()])

# build unittest from test sources with newly created lib
test = env.build_test(Glob('test/*.cpp'), add_libs = [env.module_format_name()])

# build ftxui examples
env.Append(
    LIBS = ["pthread"],
    CPPFLAGS = [
        "-Wno-unused-parameter",
        "-Wno-sign-compare",
        "-Wno-pessimizing-move",
        "-Wno-unused-function",
    ]
)

import os
for src in Glob('bin/*.cpp'):
    bin_name = os.path.basename(os.path.splitext(str(src))[0])
    env.build_bin(src, name = bin_name)

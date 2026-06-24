Import('env')

builder = env.builder()

env.Append(
    CPPFLAGS = [
        "-Wno-unused-parameter",
        "-Wno-sign-compare",
        "-Wno-pessimizing-move",
        "-Wno-unused-function",
        "-Wno-error=maybe-uninitialized",
    ]
)

# build library from lib sources - not added to environment
lib = env.build_lib(Glob('src/*.cpp'), name = env.module_format_name())

env.build_demos(Glob("demo/*.cpp"), libs = [env.module_format_name()])

# build unittest from test sources with newly created lib
test = env.build_test(Glob('test/*.cpp'), libs = [env.module_format_name()])

# build ftxui examples
env.Append(
    LIBS = ["pthread"],
)

import os
for src in Glob('bin/*.cpp'):
    bin_name = os.path.basename(os.path.splitext(str(src))[0])
    env.build_bin(src, name = bin_name)

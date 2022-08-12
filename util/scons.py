Import('env')

app = env["APP_CONFIG"]

lib = env.build_lib(Glob('src/*.cpp'))

env.build_demos(Glob("demo/*.cpp"), add_libs = [env.module_format_name()])

test = env.build_test(Glob('test/*.cpp'), add_libs = [env.module_format_name()])

Return("lib")
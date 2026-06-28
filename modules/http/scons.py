Import('env')

lib = env.build_lib(Glob('src/*.cpp') + Glob('src/navigator/*.cpp') + Glob('src/server/*.cpp') + Glob('src/curl/*.cpp'))
env.export_test(includes = ['test'], resources = ['test/resources'])

for src in Glob('demo/*.cpp'):
    name = env.file_basename(src)
    demo = env.build_demo(src, name = name, libs = [env.module_format_name()])

test = env.build_test(Glob('test/*.cpp'), libs = [env.module_format_name()])

Return('lib')

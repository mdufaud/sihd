from os.path import join, exists

Import('env')

modules = env['APP_MODULES_BUILD']

test_dir = Dir("test")
src_dir = Dir("src")

srcs = []
tests = [test_dir.File("main.cpp")]
for module in modules:
    modcap = module.capitalize()
    src_file = src_dir.File("Lua{}Api.cpp".format(modcap))
    test_file = test_dir.File("TestLua{}Api.cpp".format(modcap))
    if exists(str(src_file)):
        srcs.append(src_file)
    if exists(str(test_file)):
        tests.append(test_file)

lib = env.build_lib(srcs)
test = env.build_test(tests)

Return('lib')

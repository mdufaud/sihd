Import('env')

builder = env.builder()
conf = env.module_conf()

srcs = Glob('src/*.cpp')

lib = env.build_lib(srcs)

sihd_util_libname = env.module_format_name()

env.build_demo("demo/util_demo.cpp", name = "util_demo", libs = [sihd_util_libname])
env.build_demo("demo/scheduler_bench.cpp", name = "scheduler_bench", libs = [sihd_util_libname])

test_srcs = [f for f in Glob('test/*.cpp') if 'CppModules' not in str(f)]
test_kwargs = {}
if builder.is_cpp_modules:
	test_srcs += Glob('test/modules/*.cpp')
	test_srcs += env.build_cpp_modules('test/modules/TestGreeting.cppm')
	test_kwargs['cpp_modules'] = ['sihd.util.test.greeting']

test = env.build_test(test_srcs,
					  libs = [sihd_util_libname],
					  **test_kwargs)
Import('env')

platform = env["BUILDER_HELPER"].build_platform
include_dir = Dir("include").Dir("sihd").Dir("imgui").Dir(platform)
env.Append(CPPPATH = str(include_dir))

lib_srcs = Glob("src/{}/*.cpp".format(platform))
lib_srcs.extend(Glob("src/*.cpp"))
lib = env.build_lib(lib_srcs)

test = env.build_test()

Return('lib')

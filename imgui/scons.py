Import('env')

platform = env["BUILD_PLATFORM"]
include_dir = Dir("include").Dir("sihd").Dir("imgui").Dir(platform)
env.Append(CPPPATH = str(include_dir))

lib_srcs = Glob("src/{}/*.cpp".format(platform))
lib_srcs.extend(Glob("src/*.cpp"))
lib = env.build_lib(lib_srcs)

test = env.build_test()

Return('lib')

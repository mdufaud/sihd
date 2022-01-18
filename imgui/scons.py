Import('env')


include_dir = Dir("include").Dir("sihd").Dir("imgui")
env.Append(CPPPATH = str(include_dir))

platform = env["BUILDER_HELPER"].build_platform

lib_srcs = []
if platform == "windows":
    lib_srcs.extend(["src/imgui_impl_dx11.cpp", "src/imgui_impl_win32.cpp"])
else:
    lib_srcs.extend(["src/imgui_impl_glfw.cpp", "src/imgui_impl_opengl3.cpp"])

lib = env.build_lib(lib_srcs)
test = env.build_test()

Return('lib')

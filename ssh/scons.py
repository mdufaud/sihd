Import('env')

sihd_ssh_libname = env.module_format_name()

# libssh deprecated some functions in recent versions
# Suppress deprecation warnings until we migrate to the new APIs
env.Append(
    CPPFLAGS = [
        "-Wno-deprecated-declarations",
    ]
)

lib = env.build_lib(Glob('src/*.cpp'))
test = env.build_test(Glob('test/*.cpp'), add_libs = [sihd_ssh_libname])

env.build_demo("demo/ssh_cmd_demo.cpp", name = "ssh_cmd_demo", add_libs = [sihd_ssh_libname])
env.build_demo("demo/ssh_sftp_demo.cpp", name = "ssh_sftp_demo", add_libs = [sihd_ssh_libname])
env.build_demo("demo/ssh_shell_demo.cpp", name = "ssh_shell_demo", add_libs = [sihd_ssh_libname])
env.build_demo("demo/ssh_basic_handler_demo.cpp", name = "ssh_basic_handler_demo", add_libs = [sihd_ssh_libname])

Return('lib')

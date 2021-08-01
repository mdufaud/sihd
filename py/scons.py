from os.path import join, exists
import subprocess

Import('env')

#proc = subprocess.Popen("python3-config --extension-suffix",
#                        shell = True, stdout = subprocess.PIPE)
#proc_ret = proc.stdout.read()

#env["SHLIBPREFIX"] = "";
#env["SHLIBSUFFIX"] = proc_ret.strip();

modules = env['APP_MODULES_BUILD']
srcs = []
tests = []
for module in modules:
    modcap = module.capitalize()
    src_file = join("src", "Py{}Api.cpp".format(modcap))
    test_file = join("test", "TestPy{}Api.cpp".format(modcap))
    if exists(src_file):
        srcs.append(src_file)
    if exists(test_file):
        tests.append(test_file)

lib = env.build_lib(srcs)
test = env.build_test(tests)

Return('lib')

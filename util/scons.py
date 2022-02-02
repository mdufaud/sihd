Import('env')

app = env["APP_CONFIG"]
splitted_app_version = app.version.split('.')

env.build_replace([
    "include/sihd/util/version.hpp"
], {
    "{VERSION_MAJOR}": splitted_app_version[0],
    "{VERSION_MINOR}": splitted_app_version[1],
    "{VERSION_PATCH}": splitted_app_version[2],
})

lib = env.build_lib(Glob('src/*.cpp'), lib_name = env['APP_MODULE_FORMAT_NAME'])
test = env.build_test(Glob('test/*.cpp'), add_libs = [env['APP_MODULE_FORMAT_NAME']])

Return("lib")
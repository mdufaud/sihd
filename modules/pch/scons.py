Import('env')

env.build_header('src/pch.hpp')
env.build_header('src/pch_test.hpp', test=True)

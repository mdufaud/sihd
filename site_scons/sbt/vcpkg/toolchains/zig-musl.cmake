# Zig toolchain for musl compilation
set(CMAKE_C_COMPILER "zig" CACHE STRING "" FORCE)
set(CMAKE_CXX_COMPILER "zig" CACHE STRING "" FORCE)
set(CMAKE_C_COMPILER_ARG1 "cc -target x86_64-linux-musl" CACHE STRING "" FORCE)
set(CMAKE_CXX_COMPILER_ARG1 "c++ -target x86_64-linux-musl" CACHE STRING "" FORCE)

# Zig archiver and ranlib
set(CMAKE_AR "${CMAKE_CURRENT_LIST_DIR}/zig-ar.sh" CACHE FILEPATH "" FORCE)
set(CMAKE_RANLIB "${CMAKE_CURRENT_LIST_DIR}/zig-ranlib.sh" CACHE FILEPATH "" FORCE)

# GCC musl toolchain for x64 musl cross-compilation
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Use musl cross compilers by name (assume they are available in PATH)
# This avoids embedding user-specific absolute paths in the repo/toolchain.
set(CMAKE_C_COMPILER "x86_64-linux-musl-gcc" CACHE PATH "Musl GCC compiler")
set(CMAKE_CXX_COMPILER "x86_64-linux-musl-g++" CACHE PATH "Musl G++ compiler")
set(CMAKE_AR "x86_64-linux-musl-ar" CACHE FILEPATH "Musl ar")
set(CMAKE_RANLIB "x86_64-linux-musl-ranlib" CACHE FILEPATH "Musl ranlib")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Zig toolchain for x64 musl compilation
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Use zig as compiler
set(CMAKE_C_COMPILER "zig")
set(CMAKE_C_COMPILER_ARG1 "cc -target x86_64-linux-musl -fPIC")
set(CMAKE_CXX_COMPILER "zig")
set(CMAKE_CXX_COMPILER_ARG1 "c++ -target x86_64-linux-musl -fPIC")

# Use custom wrappers for ar and ranlib
set(CMAKE_AR "${CMAKE_CURRENT_LIST_DIR}/zig-ar.sh" CACHE FILEPATH "Archiver")
set(CMAKE_RANLIB "${CMAKE_CURRENT_LIST_DIR}/zig-ranlib.sh" CACHE FILEPATH "Ranlib")

# Disable dependency files that zig doesn't support
set(CMAKE_NINJA_CMCLDEPS_RC OFF CACHE BOOL "" FORCE)
set(CMAKE_NINJA_CMCLDEPS_C OFF CACHE BOOL "" FORCE)
set(CMAKE_NINJA_CMCLDEPS_CXX OFF CACHE BOOL "" FORCE)
# Disable linker dependency file generation
set(CMAKE_LINK_DEPENDS_USE_LINKER OFF CACHE BOOL "" FORCE)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

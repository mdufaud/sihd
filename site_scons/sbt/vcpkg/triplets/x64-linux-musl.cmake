# x64-linux-musl triplet for musl-based cross-compilation

set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)

# Use GCC musl cross-compiler
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/../toolchains/gcc-musl-x64.cmake")

set(CMAKE_C_FLAGS "-fPIC" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS "-fPIC" CACHE STRING "" FORCE)

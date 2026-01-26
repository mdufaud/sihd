# triplets/zig-arm64-linux-static.cmake

set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(VCPKG_TARGET_ARCHITECTURE aarch64)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Linux)
set(VCPKG_CMAKE_SYSTEM_PROCESSOR aarch64)
set(VCPKG_TARGET_TRIPLET "aarch64-linux-musl")

set(ARCH_TARGET "aarch64-linux-musl")

set(CMAKE_C_FLAGS "-target aarch64-linux-musl -mcpu=generic+v8a -fPIC" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS "-target aarch64-linux-musl -mcpu=generic+v8a -fPIC" CACHE STRING "" FORCE)

set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/../zig-toolchain.cmake")
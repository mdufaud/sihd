# triplets/zig-arm-linux-static.cmake

set(CMAKE_SYSTEM_PROCESSOR arm)
set(VCPKG_TARGET_ARCHITECTURE arm)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Linux)
set(ARCH_TARGET "arm-linux-musleabihf")
set(VCPKG_TARGET_TRIPLET "arm-linux-musleabihf")

# Force Position Independent Code
set(CMAKE_C_FLAGS "-target arm-linux-musleabihf -mcpu=generic+v7a -fPIC" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS "-target arm-linux-musleabihf -mcpu=generic+v7a -fPIC" CACHE STRING "" FORCE)

set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/../zig-toolchain.cmake")
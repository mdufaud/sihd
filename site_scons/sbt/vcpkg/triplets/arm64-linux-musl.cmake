# arm64-linux-musl triplet for musl-based cross-compilation

set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)

set(CMAKE_C_FLAGS "-fPIC" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS "-fPIC" CACHE STRING "" FORCE)

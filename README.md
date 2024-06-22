# SIHD

Simple Input Handler Displayer

Aimed to be a simple C++ library with some python and/or LUA bindings that gets, parse and displays data.

---

## Dependencies

### C++ 17

Used by core application.

### SCons 4.1.0

Manages compilation rules with python.

```shell
apt install scons
```

or

```shell
pip install scons
```

### python-pip

If conan is installed from python-pip, don't forget to add to ~/.bashrc:

```shell
export PATH=$PATH:$HOME/.local/bin
```

To find binary from python-pip binaries folder.

### VCPKG

Not needed if dependencies are directly installed on the building system.

Needed for Windows cross building to get DLLs or Emscripten building to get static libraries.

```shell
make dep
```

```shell
make dep mod COMMA_SEPARATED_MODULES
```

Will install vcpkg inside the repository

#### Using MSYS2

```shell
pacman -S --needed git mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-gcc
```

### Compilers

**g++ 7.5.0**

```shell
apt install g++
```

**clang++ 6.0.0**

```shell
apt install clang libc++-dev libc++abi-dev llvm
```

**g++-mingw 7.3-posix**

```shell
apt install g++-mingw-w64-x86-64
# configure mingw32 to posix
update-alternatives --config x86_64-w64-mingw32-g++
```

**emscripten 3.0.0**

```shell
apt install emscripten
```

## External libraries

All dependencies are listed in [app.py file](app.py), only few are needed for each modules

Get dependencies from conan.io center if not installed on your system

```shell
# to get all dependencies for every modules
make dep
# to be able to run unit tests
make dep test=1
# to get dependencies for single/multiple module.s
make dep mod COMMA_SEPARATED_MODULES
# to get windows dll dependencies
make dep platform=win
```

Get system external libraries list

```shell
scons pkgdep=[apt|pacman] test=[0|1] modules=COMMA_SEPARATED_MODULES
```

---

## Build

To build binaries and compile shared libraries with scons

```shell
make
```

To compile module.s

```shell
make mod COMMA_SEPARATED_MODULES
```

Add to command line for:
- static libraries: "static=1"
- address sanatizer: "asan=1"
- no debug symbols: "mode=release"
- use clang compiler: "compiler=clang"
- use mingw compiler: "compiler=mingw"
- use emscripten compiler: "compiler=em" (automatically builds with static libraries)

### Python bindings build

**python-3.6.7**

It is recommended to install it on your system with a packet manager: python3-dev

Add to your command line "py=1"

```shell
make py=1
# or
make mod core,py
```

/!\ not available with mingw windows cross compilation /!\

### Lua bindings build

**lua-5.3.5**

It is recommended to install it on your system with a packet manager: lua5.3-dev

Add to your command line "lua=1"

```shell
make lua=1
# or
make mod core,lua
```

---

## Demo

Create demo for each for the compiled module

```shell
make dep mod util demo=1
make mod util demo=1
```

---

## Tests

To compile and run tests

```shell
# Run all tests
make test
# Run some modules tests
make test COMMA_SEPARATED_MODULES
```

List tests

```shell
# For all modules
make test ls
# For some modules
make test COMMA_SEPARATED_MODULES ls
```

Filter tests

```shell
make test COMMA_SEPARATED_MODULES FILTER
```

---

## Distribution

To create pacman distribution PKGBUILD file

```shell
make dist_pacman
# for a single/multiple module.s
make dist_pacman mod COMMA_SEPARATED_MODULES
```

To create apt distribution .deb file

```shell
make dist_apt
# for a single/multiple module.s
make dist_apt mod COMMA_SEPARATED_MODULES
```

To create a tar.gz with current build

```shell
make dist_tar
# for a single/multiple module.s
make dist_tar mod COMMA_SEPARATED_MODULES
```

---

## Installation

To install build into your system **/usr/local**

```shell
make install
# to change default install directory from /usr/local/lib -> pkg/usr/local/lib ...
make install INSTALL_DESTDIR=pkg
# to change default /usr/local to /usr
make install INSTALL_PREFIX=/usr
```

This will create into Makefile's folder a '.installed' containing every file installed

This file is used by 'uninstall' makefile rule to remove what has been installed.

```shell
make uninstall
```

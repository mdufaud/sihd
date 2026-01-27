# SIHD

Simple Input Handler Displayer

Aimed to be a simple C++ library with some python and/or LUA bindings that gets, parse and displays data.

---

## Dependencies

### C++ 20

Used by core application.

### SCons 4.1.0

Manages compilation rules with python.

```shell
# recommended
apt install python3-pip
pip install scons
# or
apt install scons
```

If scons is installed from python-pip, don't forget to add to ~/.bashrc:

```shell
export PATH=$PATH:$HOME/.local/bin
```

To find binary from python-pip binaries folder.

### VCPKG (external libraries)

All dependencies are listed in [app.py file](app.py), only few are needed for each modules

#### Install globally on your system

This step is not needed if dependencies are directly installed on the building system and you don't need cross compiling.

```shell
# prints system external libraries list for a specific package manager
scons pkgdep=[apt|pacman] test=[0|1] modules=COMMA_SEPARATED_MODULES
```

#### Compile locally for architecture/cross building

Needed for Windows cross building or Emscripten building to get static libraries or if you don't want to install libraries on your system.



```shell
# needed by vcpkg
apt install curl zip unzip tar cmake ninja-build
```

Installs vcpkg inside the repository (no leaking in your system)

```shell
# to get all dependencies for every modules
make dep
# to be able to run unit tests
make dep test=1
# to get dependencies for single/multiple module.s
make dep mod COMMA_SEPARATED_MODULES
# to get windows dll dependencies
make dep platform=win
# for lua/python bindings
make dep lua=1 python=1
```

#### Using MSYS2

If you want to compile for windows using MSYS2

```shell
pacman -S --needed git mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-gcc
```

### Compilers

**g++ 11.4.0**

```shell
apt install g++
```

**clang++ 18.1.3**

```shell
apt install clang libc++-dev libc++abi-dev llvm
```

**g++-mingw 13-posix**

```shell
apt install g++-mingw-w64-x86-64
# configure mingw32 to posix
update-alternatives --config x86_64-w64-mingw32-g++
```

**emscripten 3.0.0**

```shell
apt install emscripten
```

```shell
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
# add to bashrc
source ./emsdk_env.sh
```

---

## Build

To build binaries and compile shared libraries with scons

```shell
make
```

To compile specific module.s

```shell
make mod COMMA_SEPARATED_MODULES
```

Flavours (you can mix them mostly)

```shell
# static libraries
make static=1
# address sanitizer
make asan=1
# no debug symbols
make mode=release
# build with clang
make compiler=clang
# build with emscripten (automatically builds with static libraries)
make compiler=em
# build for windows
make platform=win
```

### Python bindings build

**python-3.6.7**

It is recommended to install it on your system with a package manager: `python3-dev`

Add to your command line "py=1"

```shell
make py=1
# or
make mod core,py
```

/!\ not available with mingw windows cross compilation /!\

### Lua bindings build

**lua-5.3.5**

It is recommended to install it on your system with a package manager: `lua5.3-dev`

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
make dep mod COMMA_SEPARATED_MODULES demo=1
make demo COMMA_SEPARATED_MODULES
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

Launch only specific tests

```shell
make test COMMA_SEPARATED_MODULES FILTER
```

Repeat tests

```shell
make test repeat=NUMBER
```

Execute non interactive tests

```shell
make itest
```

Debug tests

```shell
# gdb
make gtest
# valgrind
make vtest
# compiled with libasan
make stest
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

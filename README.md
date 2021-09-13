# SIHD

Simple Input Handler Displayer

Aimed to be a simple C++ library with some python and/or LUA bindings that gets, parse and display data

## Dependencies

### C++

version 17

### SCons

SCons version v4.1.0 (builder)

```shell
pip install scons
```

### Conan

Conan version 1.35.0 (manages dependencies)

[Conan install doc](https://docs.conan.io/en/latest/installation.html)

```shell
pip install conan
```

### Python-pip

Don't forget to add to your bashrc:

```shell
export PATH=$PATH:$HOME/.local/bin
```

To find conan and scons from python-pip binaries folder.

### Compilers

g++ (GCC) 7.5.0

```shell
sudo apt install g++
```

clang++ 6.0.0

```shell
# to install
sudo apt install libc++-dev libc++abi-dev
```

mingw 7.3-posix

```shell
# configure mingw32 to posix
sudo update-alternatives --config x86_64-w64-mingw32-g++
```

### External libraries

All dependencies are listed in [app.py file](app.py), only few are needed for each modules

Get dependencies from conan.io center if not installed on your system

```shell
# to get all dependencies for every modules
make dep
# to be able to run unit tests
make dep test=1
# to get depencencies compiled with clang
make dep compiler=clang
# to get dependencies for a single module
make dep mod COMMA_SEPARATED_MODULES
# to get windows dependencies
make dep platform=win
```

To get libraries compiled with clang, add to your command line: "compiler=clang"

To get libraries compiled for windows (dll), add to your command line: "platform=win"

## Build

To build binaries and compile shared libraries with scons

```shell
make
```

To compile specific or single module

```shell
make modules=COMMA_SEPARATED_MODULES
# or
make mod COMMA_SEPARATED_MODULES
```

To compile with clang, add to your command line: "compiler=clang"

To compile with mingw, add to your command line: "compiler=mingw"

### Python bindings build

python-3.6.9

Add to your command line "py=1"

```shell
make py=1
# or
make mod core,py
```

/!\ not available with mingw /!\

### Lua bindings build

lua-5.3.5

Add to your command line "lua=1"

```shell
make lua=1
# or
make mod core,lua
```

## Tests

To compile and run tests

```shell
# Run all tests
make test
# Run all module's tests
make test COMMA_SEPARATED_MODULES
```

List tests

```shell
# For all
make test ls
# For a single module
make test COMMA_SEPARATED_MODULES ls
```

Filter tests

```shell
make test m=COMMA_SEPARATED_MODULES t=FILTER
# or
make test COMMA_SEPARATED_MODULES FILTER
```

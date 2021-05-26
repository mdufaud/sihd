# SIHD

Simple Input Handler Displayer

Aimed to be a simple C++ library with some python and/or LUA bindings that gets, parse and display data

## How to compile

### SCons

SCons version v4.1.0

```shell
pip install scons
```

### Conan

Conan version 1.35.0

[Conan install doc](https://docs.conan.io/en/latest/installation.html)

```shell
pip install conan
```

### python-pip

Don't forget to add to your bashrc:

```shell
export PATH=$PATH:$HOME/.local/bin
```

To be able to find conan and scons from pip.

### C++

version 17

### Compiler

g++ (GCC) 10.2.0

### Build instructions

To dependencies from conan.io center

```shell
# To get all dependencies for every modules
make dep
# To be able to run unit tests
make dep test
# To get dependencies for a single module
make dep mod COMMA_SEPARATED_MODULES
```

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

Activate verbose by adding verbose=1

### Python bindings build

Add to your command line "py=1"

```shell
make mod core py=1
# or
make mod core,py
```

### Lua bindings build

Add to your command line "lua=1"

```shell
make mod core lua=1
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

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
make install
# To be able to run unit tests
make install_test
# To get dependencies for a single module
make install_module MODULE
```

To build binaries and compile shared libraries with scons

```shell
make
```

To compile a single module

```shell
make module=MODULE
# or
make module MODULE
```

Activate verbose by adding verbose=1

### Python bindings build

Its time consumming

## Tests

To compile and run tests

```shell
# Run all tests
make test
# Run all module's tests
make test MODULE
```

List tests

```shell
# For all
make test ls
# For a single module
make test MODULE ls
```

Filter tests

```shell
make test m=MODULE t=FILTER
# or
make test MODULE FILTER
```

# SIHD

Simple Input Handler Displayer

Aimed to be a simple C++ library with some python and/or LUA bindings that gets, parse and display data

## How to compile

### Scons

Scons version v4.1.0

```shell
pip install scons
```

### Conan

Conan version 1.35.0

[Conan install doc](https://docs.conan.io/en/latest/installation.html)

modify: ~/.conan/profiles/default

compiler.libcxx=libstdc++11

### C++

version 20

### Compiler

g++ (GCC) 10.2.0

### Build instructions

To pull every dependencies

```shell
# For building only
make install
# For running tests
make install_test
# For building a single module (and its module dependencies)
make install_module MODULE
```

To build binaries and compile shared libraries

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

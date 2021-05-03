# SIHD

## Scons

Scons version v4.1.0

```shell
pip install scons
```

## Conan

Conan version 1.35.0

[Conan install doc](https://docs.conan.io/en/latest/installation.html)

modify: ~/.conan/profiles/default

compiler.libcxx=libstdc++11

## C++

version 20

## Compiler

g++ (GCC) 10.2.0

## Installation

To pull every dependencies then build binaries and compile shared libraries

```shell
make
```

To pull every dependencies

```shell
make install
```

To compile a single module

```shell
make module=MODULE
```

Activate verbose by adding verbose=1

## Run tests

To compile and run all modules tests

```shell
make test
```

For a single test

```shell
make test m=MODULE t=TESTNAME
```

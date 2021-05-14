OS := $(shell uname)

ifeq ($(OS),Linux)
	SHELL := /bin/bash
	GREP := /bin/grep
else
	SHELL := /bin/sh
	GREP := /usr/bin/grep
endif

APP_NAME = sihd

HERE = $(shell pwd)

# Build
BUILD_PATH = $(HERE)/build
LIB_PATH = $(BUILD_PATH)/lib
INCLUDE_PATH = $(BUILD_PATH)/include
TEST_PATH = $(BUILD_PATH)/test
BIN_PATH = $(BUILD_PATH)/bin
OBJ_PATH = $(BUILD_PATH)/obj
RES_PATH = $(BUILD_PATH)/etc
BUILD_UTILS = $(HERE)/.build_utils

# Scons
SCONS_BUILD_CMD = scons -Q -j4

# Conan
EXTLIB_PATH = $(BUILD_PATH)/extlib
CONAN_PATH = $(BUILD_PATH)/conan
CONAN_INSTALL = conan install . --profile .conan_profile -if $(CONAN_PATH)

#########
# RULES #
#########

export APP_NAME
export TEST_PATH
export EXTLIB_PATH
export BIN_PATH
export RES_PATH

all: build

#
# Conan (external libraries dependencies retrieval)
#

install:
	$(CONAN_INSTALL)

#
# Scons (builder)
#

build:
	@$(SCONS_BUILD_CMD) verbose=$(verbose) module=$(module) test=$(test) dist=$(dist)

verbose: verbose = 1
verbose: build

ifeq ($(word 1, $(MAKECMDGOALS)), module)
MODULE_NAME=$(word 2, $(MAKECMDGOALS))$(m)

module: module = $(MODULE_NAME)
module: build

# for no 'no rules to make...'
$(MODULE_NAME):

endif

#
# Test
#

TEST_EXEC=./$(TEST_PATH)/*
TEST_ARGS=--gtest_break_on_failure

# find string 'test' in target
ifneq ($(findstring test,$(word 1, $(MAKECMDGOALS))), )
# handles:
#	make test
#	make test MODULE FILTER
#	make test MODULE ls
#	make test ls
#	make test t=FILTER
MODULE_NAME=$(word 2, $(MAKECMDGOALS))$(m)
TEST_NAME=$(word 3, $(MAKECMDGOALS))$(t)

ifneq ($(MODULE_NAME), )
	TEST_EXEC=$(TEST_PATH)/$(APP_NAME)_$(MODULE_NAME)
endif

# case: make test ls
ifeq ($(MODULE_NAME),ls)
ifeq ($(TEST_NAME), )
	TEST_EXEC=$(TEST_PATH)/*
	TEST_NAME=ls
endif
endif

# ls to list tests, else filter tests
ifeq ($(TEST_NAME),ls)
	TEST_ARGS+=--gtest_list_tests
else
	TEST_ARGS+=--gtest_filter="*$(TEST_NAME)*"
endif

ifneq ($(MODULE_NAME), )
test: module = $(MODULE_NAME)
endif

# for no 'no rules to make...'
$(MODULE_NAME):

# for no 'no rules to make...'
$(TEST_NAME):

endif

test: test = 1
test: build
	@echo "Running test: $(TEST_EXEC) $(TEST_ARGS)"
	@env $(DEBUGGER) $(TEST_EXEC) $(TEST_ARGS)

valgrindtest: DEBUGGER = valgrind --leak-check=full
valgrindtest: test

gdbtest: DEBUGGER = gdb
gdbtest: test

#
# Distribution
#

dist: dist = 1
dist: build

#
# Builder
#

include $(BUILD_UTILS)/rules.mk

#
# Cleanup
#

clean:
	@echo "Removing $(APP_NAME) compilation build"
	@rm -rf $(LIB_PATH) $(INCLUDE_PATH) $(TEST_PATH) $(OBJ_PATH) $(BIN_PATH) && echo "Done" || echo "Failed"

cleaninstall:
	@echo "Removing $(APP_NAME) dependencies"
	@rm -rf $(CONAN_PATH) $(EXTLIB_PATH) && echo "Done" || echo "Failed"

fclean:
	@echo "Removing build"
	@rm -rf $(BUILD_PATH)

### Makefile
.PHONY: install build verbose test valgrindtest gdbtest dist fclean clean cleaninstall
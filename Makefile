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
BUILD_TOOLS = $(HERE)/_build_tools

# Scons
SCONS_BUILD_CMD = scons -Q -j4

# Conan
EXTLIB_PATH = $(BUILD_PATH)/extlib
CONAN_PATH = $(BUILD_PATH)/conan
CONAN_DEP = conan install .
CONAN_PROFILE_LIBSTDC = $(shell conan profile get settings.compiler.libcxx default)
CONAN_DEP_PROFILE = --profile default
CONAN_DEP_PATH = -if $(CONAN_PATH) 

#########
# Rules 
#########

export APP_NAME
export TEST_PATH
export EXTLIB_PATH
export BIN_PATH
export RES_PATH

all: build

#########
# Conan (external libraries dependencies retrieval)
#########

ifeq ($(word 1, $(MAKECMDGOALS)), dep)
.PHONY: dep
dep:
ifneq ($(CONAN_PROFILE_LIBSTDC), libstdc++11)
	@echo "Conan profile compiler.libcxx is not libstdc++11 (is $(CONAN_PROFILE_LIBSTDC))"
	@echo "Updating default profile to libstdc++11"
	@conan profile update settings.compiler.libcxx="libstdc++11" default
endif
	@env test=$(test) module=$(module) $(CONAN_DEP) $(CONAN_DEP_PROFILE) $(CONAN_DEP_PATH)

# make dep module MODULE
ifeq ($(word 2, $(MAKECMDGOALS)), module)
MODULE_NAME=$(word 3, $(MAKECMDGOALS))$(m)
dep: module = $(MODULE_NAME)
module:
$(MODULE_NAME):
endif

# make dep test
ifeq ($(word 2, $(MAKECMDGOALS)), test)
dep: test = 1
test:
endif

endif # dep

checkdep_html:
	@conan info . --graph=$(CONAN_PATH)/dep_tree.html

checkdep:
	@conan info . --graph=$(CONAN_PATH)/dep_tree.txt && cat $(CONAN_PATH)/dep_tree.txt

########
# Scons (builder)
########

build:
	@$(SCONS_BUILD_CMD) verbose=$(verbose) module=$(module) test=$(test) dist=$(dist)

build_debug: SCONS_BUILD_CMD = time scons --debug=count,duplicate,explain,findlibs,includes,memoizer,memory,objects,prepare,presub,stacktrace,time
build_debug: build

verbose: verbose = 1
verbose: build

# make module MODULE
ifeq ($(word 1, $(MAKECMDGOALS)), module)
.PHONY: module
MODULE_NAME=$(word 2, $(MAKECMDGOALS))$(m)
module: module = $(MODULE_NAME)
module: build
$(MODULE_NAME):
endif # module

########
# Test
########

TEST_EXEC=$(TEST_PATH)/*
TEST_ARGS=--gtest_break_on_failure

# find string 'test' in target
ifneq ($(findstring test,$(word 1, $(MAKECMDGOALS))), )

test: test = 1
test: build
	@for test_bin in $(TEST_EXEC); do \
		echo "Running test: $$test_bin $(TEST_ARGS)" ; \
		env $(DEBUGGER) $$test_bin $(TEST_ARGS) ; \
	done

valgrindtest: DEBUGGER = valgrind --leak-check=full
valgrindtest: test

gdbtest: DEBUGGER = gdb
gdbtest: test

.PHONY: test valgrindtest gdbtest

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

$(MODULE_NAME):
$(TEST_NAME):
endif #test

##############
# Distribution
##############

dist: dist = 1
dist: build

##########
# Builder
##########

include $(BUILD_TOOLS)/rules.mk

##########
# Cleanup
##########

clean:
	@echo "Removing $(APP_NAME) compilation build"
	@rm -rf $(LIB_PATH) $(INCLUDE_PATH) $(TEST_PATH) $(OBJ_PATH) $(BIN_PATH) $(RES_PATH) && echo "Done" || echo "Failed"

cleaninstall:
	@echo "Removing $(APP_NAME) dependencies"
	@rm -rf $(CONAN_PATH) $(EXTLIB_PATH) && echo "Done" || echo "Failed"

fclean:
	@echo "Removing build"
	@rm -rf $(BUILD_PATH)

### Makefile
.PHONY: install build verbose dist fclean clean cleaninstall
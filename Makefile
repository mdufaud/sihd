OS := $(shell uname)

ifeq ($(OS),Linux)
	SHELL := /bin/bash
	GREP := /bin/grep
else
	SHELL := /bin/sh
	GREP := /usr/bin/grep
endif

APPNAME = sihd

# Build
BUILD_PATH = build
LIB_PATH = $(BUILD_PATH)/lib
INCLUDE_PATH = $(BUILD_PATH)/include
TEST_PATH = $(BUILD_PATH)/test
BIN_PATH = $(BUILD_PATH)/bin
OBJ_PATH = $(BUILD_PATH)/obj
BUILD_UTILS = .build_utils

# Scons
SCONS_BUILD_CMD = scons -Q -j4

# Conan
EXTLIB_PATH = $(BUILD_PATH)/extlib
CONAN_PATH = $(BUILD_PATH)/conan
CONAN_INSTALL = conan install . --profile .conan_profile -if $(CONAN_PATH)

#########
# RULES #
#########

all: build

#
# Conan (external libraries dependencies retrieval)
#

define build_app
	$(SCONS_BUILD_CMD) verbose=$(verbose) module=$(module) test=$(test) dist=$(dist)
endef 

install: $(EXTLIB_PATH)

$(EXTLIB_PATH):
	$(CONAN_INSTALL)

#
# Scons (builder)
#

build: install
	$(call build_app)

verbose: install
	$(call build_app) verbose=1

ifeq ($(word 1, $(MAKECMDGOALS)), module)
MODULE_NAME=$(word 2, $(MAKECMDGOALS))$(m)

module: install
	$(call build_app) module=$(MODULE_NAME)

# for no 'no rules to make...'
$(MODULE_NAME):

endif

#
# Test
#

# handles:
#	make test
#	make test MODULE FILTER
#	make test MODULE ls
#	make test ls
#	make test t=FILTER
ifeq ($(word 1, $(MAKECMDGOALS)), test)
MODULE_NAME=$(word 2, $(MAKECMDGOALS))$(m)
TEST_NAME=$(word 3, $(MAKECMDGOALS))$(t)
TEST_ARGS=--gtest_break_on_failure
ifeq ($(MODULE_NAME), )
	TEST_EXEC=./$(TEST_PATH)/*
else
	TEST_EXEC=./$(TEST_PATH)/$(APPNAME)_$(MODULE_NAME)
endif

# case: make test ls
ifeq ($(MODULE_NAME),ls)
ifeq ($(TEST_NAME), )
	TEST_EXEC=./$(TEST_PATH)/*
	TEST_NAME=ls
endif
endif

# ls to list tests, else filter it
ifeq ($(TEST_NAME),ls)
	TEST_ARGS+=--gtest_list_tests
else
	TEST_ARGS+=--gtest_filter="*$(TEST_NAME)*"
endif

.PHONY: test
test: $(TEST_PATH) $(TEST_PATH)/$(APP_NAME)_$(MODULE_NAME)

$(TEST_PATH)/$(APP_NAME)_$(MODULE_NAME):
	$(call build_app) test=1 module=$(MODULE_NAME) \
	&& $(TEST_EXEC) $(TEST_ARGS)

$(TEST_PATH):
	$(call build_app) test=1 module=$(MODULE_NAME) \
	&& $(TEST_EXEC) $(TEST_ARGS)

# for no 'no rules to make...'
$(MODULE_NAME):

# for no 'no rules to make...'
$(TEST_NAME):

endif

#
# Distribution
#

dist:
	$(call build_app) dist=1

#
# Builder
#

ifeq ($(word 1, $(MAKECMDGOALS)), newmod)
MODULE_NAME=$(word 2, $(MAKECMDGOALS))$(m)

newmod:
	bash $(BUILD_UTILS)/make_module.sh $(APPNAME) $(MODULE_NAME)

# for no 'no rules to make...'
$(MODULE_NAME):

endif

ifeq ($(word 1, $(MAKECMDGOALS)), newtest)
MODULE_NAME=$(word 2, $(MAKECMDGOALS))$(m)
TEST_NAME=$(word 3, $(MAKECMDGOALS))$(t)

newtest:
	bash $(BUILD_UTILS)/make_test.sh $(APPNAME) $(MODULE_NAME) $(TEST_NAME)

# for no 'no rules to make...'
$(MODULE_NAME):

# for no 'no rules to make...'
$(TEST_NAME):

endif


ifeq ($(word 1, $(MAKECMDGOALS)), newclass)
MODULE_NAME=$(word 2, $(MAKECMDGOALS))$(m)
CLASS_NAME=$(word 3, $(MAKECMDGOALS))$(c)

newclass:
	bash $(BUILD_UTILS)/make_class.sh $(APPNAME) $(MODULE_NAME) $(CLASS_NAME)

# for no 'no rules to make...'
$(MODULE_NAME):

# for no 'no rules to make...'
$(CLASS_NAME):

endif

ifeq ($(word 1, $(MAKECMDGOALS)), newnamedclass)
MODULE_NAME=$(word 2, $(MAKECMDGOALS))$(m)
CLASS_NAME=$(word 3, $(MAKECMDGOALS))$(c)

newnamedclass:
	bash $(BUILD_UTILS)/make_named_class.sh $(APPNAME) $(MODULE_NAME) $(CLASS_NAME)

# for no 'no rules to make...'
$(MODULE_NAME):

# for no 'no rules to make...'
$(CLASS_NAME):

endif

#
# Cleanup
#

clean:
	echo "Removing $(APPNAME) compilation build"
	rm -rf $(LIB_PATH) $(INCLUDE_PATH) $(TEST_PATH) $(OBJ_PATH) $(BIN_PATH) && echo "Done" || echo "Failed"

cleaninstall:
	echo "Removing $(APPNAME) dependencies"
	rm -rf $(CONAN_PATH) $(EXTLIB_PATH) && echo "Done" || echo "Failed"

fclean:
	echo "Removing build"
	rm -rf $(BUILD_PATH)

### Makefile
.PHONY: install build buildv test dist fclean clean cleaninstall
.IGNORE:
.SILENT:
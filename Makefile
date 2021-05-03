ARGS = $(foreach a,$($(subst -,_,$1)_args),$(if $(value $a),$a="$($a)"))

OS := $(shell uname)

ifeq ($(OS),Linux)
	SHELL := /bin/bash
	GREP := /bin/grep
else
	SHELL := /bin/sh
	GREP := /usr/bin/grep
endif

NAME = sihd

# Build
BUILD_PATH = build
LIB_PATH = $(BUILD_PATH)/lib
INCLUDE_PATH = $(BUILD_PATH)/include
TEST_PATH = $(BUILD_PATH)/test
BIN_PATH = $(BUILD_PATH)/bin
OBJ_PATH = $(BUILD_PATH)/obj
BUILD_UTILS = .build_utils

# Scons
BUILD_CMD = scons -Q -j4

# Conan
EXTLIB_PATH = $(BUILD_PATH)/extlib
CONAN_PATH = $(BUILD_PATH)/conan
CONAN_INSTALL = conan install . --profile .conan_profile -if $(CONAN_PATH)

#########
# RULES #
#########

all: build

### Conan (external libraries dependencies retrieval) ###

install: $(EXTLIB_PATH)

$(EXTLIB_PATH):
	$(CONAN_INSTALL)

### Scons (builder) ###

build: install
	$(BUILD_CMD) verbose=$(verbose) module=$(module)

### Test ###

test: $(TEST_PATH)
	./$(TEST_PATH)/*

$(TEST_PATH): build

### Distribution ###

dist: install
	BUILD_ARGS="dist=1" $(BUILD_CMD)

### Builder ###

newmod:
	echo $(ARGS)
	bash $(BUILD_UTILS)/make_module.sh $(NAME) $(m)

newtest:
	bash $(BUILD_UTILS)/make_test.sh $(NAME) $(m) $(t)

newclass:
	bash $(BUILD_UTILS)/make_class.sh $(NAME) $(m) $(c)

### Cleanup ###

clean:
	echo "Removing $(NAME) compilation build"
	rm -rf $(LIB_PATH) $(INCLUDE_PATH) $(TEST_PATH) $(OBJ_PATH) $(BIN_PATH) && echo "Done" || echo "Failed"

cleaninstall:
	echo "Removing $(NAME) dependencies"
	rm -rf $(CONAN_PATH) $(EXTLIB_PATH) && echo "Done" || echo "Failed"

fclean: clean cleaninstall

### Makefile
.PHONY: install build buildv test dist fclean clean cleaninstall
.IGNORE:
.SILENT:
OS := $(shell uname)

ifeq ($(OS),Linux)
	SHELL := /bin/bash
	GREP := /bin/grep
else
	SHELL := /bin/sh
	GREP := /usr/bin/grep
endif

APP_NAME = sihd

HERE = $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

# Build
BUILD_PATH = $(HERE)/build
LIB_PATH = $(BUILD_PATH)/lib
INCLUDE_PATH = $(BUILD_PATH)/include
TEST_PATH = $(BUILD_PATH)/test
TEST_BIN_PATH = $(TEST_PATH)/bin
BIN_PATH = $(BUILD_PATH)/bin
OBJ_PATH = $(BUILD_PATH)/obj
RES_PATH = $(BUILD_PATH)/etc
BUILD_TOOLS = $(HERE)/_build_tools

# Scons
SCONS_BUILD_CMD = scons -Q -j$(UTILS_LOGICAL_CORE_NUMBER)

# Conan
EXTLIB_PATH = $(BUILD_PATH)/extlib
CONAN_PATH = $(BUILD_PATH)/conan
CONAN_INSTALL = conan install $(HERE)
CONAN_INSTALL_PATH = -if $(CONAN_PATH) 

#########
# Rules 
#########

export APP_NAME
export TEST_PATH
export LIB_PATH
export EXTLIB_PATH
export BIN_PATH
export RES_PATH

all: build

##########
# Includes
##########

include $(BUILD_TOOLS)/logger.mk
include $(BUILD_TOOLS)/utils.mk
include $(BUILD_TOOLS)/rules.mk

#########
# Conan (external libraries dependencies retrieval)
#########

ifeq ($(word 1, $(MAKECMDGOALS)), dep)
.PHONY: dep
dep:
	@env test=$(test) modules=$(modules) lua=$(lua) py=$(py) $(CONAN_INSTALL) $(CONAN_INSTALL_PATH) $(args)

# make dep mod MODULE
ifeq ($(word 2, $(MAKECMDGOALS)), mod)
MODULES_NAME = $(word 3, $(MAKECMDGOALS))$(m)
dep: modules = $(MODULES_NAME)
mod:
$(MODULES_NAME):
endif

# make dep test
ifeq ($(word 2, $(MAKECMDGOALS)), test)
dep: test = 1
test:
endif

# make dep build
ifeq ($(word 2, $(MAKECMDGOALS)), build)
dep: CONAN_ARGS = --build
build:
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
	@cd $(HERE) && env verbose=$(verbose) modules=$(modules) test=$(test) dist=$(dist) py=$(py) lua=$(lua) $(SCONS_BUILD_CMD)

build_debug: SCONS_BUILD_CMD = time scons --debug=count,duplicate,explain,findlibs,includes,memoizer,memory,objects,prepare,presub,stacktrace,time
build_debug: build

verbose: verbose = 1
verbose: build

# make mod MODULE
ifeq ($(word 1, $(MAKECMDGOALS)), mod)
.PHONY: mod
MODULES_NAME = $(word 2, $(MAKECMDGOALS))$(m)
mod: modules = $(MODULES_NAME)
mod: build
$(MODULES_NAME):
endif # module

########
# Test
########

get-module-name = $(word 2, $(subst _, , $(basename $1)))

TEST_EXEC = $(wildcard $(TEST_BIN_PATH)/*)
TEST_ARGS =

# find string 'test' in target
ifneq ($(findstring test,$(word 1, $(MAKECMDGOALS))), )


test: test = 1
test: build
	@- $(foreach TEST_BIN, $(TEST_EXEC), \
		$(eval TEST_CMD_LINE = \
			env $(DEBUGGER) $(TEST_BIN) $(TEST_ARGS)\
		) \
		$(eval TEST_MODULE_NAME = $(call get-module-name, $(TEST_BIN))) \
		$(eval TEST_MODULE_PATH = $(HERE)/$(TEST_MODULE_NAME)/test) \
		$(eval export TEST_MODULE_PATH) \
		cd $(HERE)/$(TEST_MODULE_NAME); \
		echo "Tested module: $(TEST_MODULE_NAME)" ; \
		echo "Running command: $(TEST_CMD_LINE)" ; \
		$(TEST_CMD_LINE); \
		cd - > /dev/null ;\
	)

valgrindtest: DEBUGGER = valgrind --leak-check=full --show-leak-kinds=all
valgrindtest: test
vtest: DEBUGGER = valgrind --leak-check=full --show-leak-kinds=all
vtest: test

gdbtest: DEBUGGER = gdb
gdbtest: test

.PHONY: test vtest valgrindtest gdbtest

# handles:
#	make test
#	make test MODULE FILTER
#	make test MODULE ls
#	make test ls
#	make test t=FILTER
COMMA = ,
MODULES_NAME = $(word 2, $(MAKECMDGOALS))$(m)
MODULES_NAME_SPLIT = $(subst $(COMMA), ,$(MODULES_NAME))
TEST_NAME = $(word 3, $(MAKECMDGOALS))$(t)
TEST_ARGS = --gtest_death_test_style=threadsafe --gtest_shuffle

ifneq ($(MODULES_NAME), )
	TEST_EXEC = $(foreach var, $(MODULES_NAME_SPLIT), $(TEST_BIN_PATH)/$(APP_NAME)_$(var))
endif

# case: make test ls
ifeq ($(MODULES_NAME),ls)
ifeq ($(TEST_NAME), )
	TEST_EXEC = $(wildcard $(TEST_BIN_PATH)/*)
	TEST_NAME = ls
	MODULES_NAME = 
endif
endif

# ls to list tests, else filter tests
ifeq ($(TEST_NAME),ls)
	TEST_ARGS += --gtest_list_tests
else
	TEST_ARGS += --gtest_filter="*$(TEST_NAME)*"
endif

ifneq ($(MODULES_NAME), )
test: modules = $(MODULES_NAME)
endif

$(MODULES_NAME):
$(TEST_NAME):
endif #test

##############
# Distribution
##############

dist: dist = 1
dist: build

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

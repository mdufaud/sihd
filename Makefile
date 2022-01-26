ifeq (, $(shell which python3))
$(error "Makefile: no python3 detected - it is needed to build the project.")
endif

ifeq (, $(shell which scons))
$(error "Makefile: no python-scons detected - it is needed to build the project.")
endif

APP_NAME = sihd
OS := $(shell uname)
HERE = $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

ifeq ($(OS),Linux)
SHELL := /bin/bash
GREP := /bin/grep
else
SHELL := /bin/sh
GREP := /usr/bin/grep
endif

BUILD_TOOLS = $(HERE)/_build_tools
MAKEFILE_TOOLS = $(BUILD_TOOLS)/makefile

BUILD_EXTRA = $(HERE)/_build_extra
MAKEFILE_EXTRA = $(BUILD_EXTRA)/makefile

BUILDER = $(BUILD_TOOLS)/builder.py

# Builder + env conf

BUILDER_RESP = $(shell arch=$(arch) mode=$(mode) platform=$(platform) compiler=$(compiler) python3 $(BUILDER) all)
PLATFORM = $(word 1, $(BUILDER_RESP))
ARCH = $(word 2, $(BUILDER_RESP))
COMPILE_MODE = $(word 3, $(BUILDER_RESP))
COMPILER = $(word 4, $(BUILDER_RESP))
ANDROID = $(word 5, $(BUILDER_RESP))

# Build path

BUILD_ENTRY_PATH = $(HERE)/build
BUILD_PATH = $(BUILD_ENTRY_PATH)/$(PLATFORM)-$(ARCH)/$(COMPILE_MODE)
EXTLIB_PATH = $(BUILD_PATH)/extlib
EXTLIB_LIB_PATH = $(EXTLIB_PATH)/lib
LIB_PATH = $(BUILD_PATH)/lib
INCLUDE_PATH = $(BUILD_PATH)/include
TEST_PATH = $(BUILD_PATH)/test
TEST_BIN_PATH = $(TEST_PATH)/bin
BIN_PATH = $(BUILD_PATH)/bin
OBJ_PATH = $(BUILD_PATH)/obj
RES_PATH = $(BUILD_PATH)/etc

# Dist path
DIST_PATH = $(HERE)/dist

# Scons
j = $(UTILS_LOGICAL_CORE_NUMBER)
SCONS_BUILD_CMD = $(SCONS_PREFIX) scons -Q -j$(j) $(SCONS_ARGS)

# Conan
CONAN_PATH = $(BUILD_PATH)/conan
CONAN_PROFILES_PATH = $(BUILD_TOOLS)/conan_profiles
CONAN_PROFILE = $(CONAN_PROFILES_PATH)/$(ARCH)/$(COMPILER).txt
CONAN_PROFILE_ARG =
CONAN_INSTALL = conan install $(HERE) -if $(CONAN_PATH) $(CONAN_INSTALL_PATH) $(CONAN_PROFILE_ARG) $(CONAN_ARGS)
# checking platform env to select a conan profile
ifneq ("$(wildcard $(CONAN_PROFILE))", "")
	CONAN_PROFILE_ARG = --profile $(CONAN_PROFILE)
endif

##########
# Includes
##########

include $(MAKEFILE_TOOLS)/logger.mk
include $(MAKEFILE_TOOLS)/utils.mk
include $(MAKEFILE_TOOLS)/templates.mk

include $(MAKEFILE_EXTRA)/extra.mk

#########
# Exports
#########

export APP_NAME
export TEST_PATH
export BUILD_PATH
export LIB_PATH
export BIN_PATH
export RES_PATH
export EXTLIB_PATH
export EXTLIB_LIB_PATH

#########
# Targets
#########

all: build

intro:
	$(call log_info,makefile,project: $(APP_NAME))
	$(call log_info,makefile,platform = $(PLATFORM))
	$(call log_info,makefile,compiler = $(COMPILER))
	$(call log_info,makefile,arch = $(ARCH))
	$(call log_info,makefile,mode = $(COMPILE_MODE))
	$(call log_info,makefile,logical cores = $(UTILS_LOGICAL_CORE_NUMBER))
ifeq ($(ANDROID), true)
	$(call log_warning,makefile,android detected)
endif

########
# Scons (builder)
########

build: intro
	$(call log_info,makefile,starting build with command: $(SCONS_BUILD_CMD))
	@cd $(HERE) && env verbose=$(verbose) \
						modules=$(modules) \
						test=$(test) \
						dist=$(dist) \
						py=$(py) \
						lua=$(lua) \
						asan=$(asan) \
						static=$(static) \
						j=$(j) \
						$(SCONS_BUILD_CMD)

build_debug: SCONS_ARGS = --debug=count,duplicate,explain,findlibs,includes,memoizer,memory,objects,prepare,presub,stacktrace,time
build_debug: SCONS_PREFIX = time
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

get-module-name = $(word 2, $(subst _, , $(notdir $1)))

TEST_EXEC = $(wildcard $(TEST_BIN_PATH)/*)
TEST_ARGS =
DEBUGGER_ARGS =

# find string 'test' in target
ifneq ($(findstring test,$(word 1, $(MAKECMDGOALS))), )

test: test = 1
test: build
	$(call log_info,makefile,starting tests in build: $(TEST_PATH))
	@- $(foreach TEST_BIN, $(TEST_EXEC), \
		$(eval TEST_CMD_LINE = \
			env $(DEBUGGER) $(DEBUGGER_ARGS) $(TEST_BIN) $(TEST_ARGS)\
		) \
		$(eval TEST_MODULE_NAME = $(call get-module-name, $(TEST_BIN))) \
		cd $(HERE)/$(TEST_MODULE_NAME); \
		echo testing module: $(TEST_MODULE_NAME); \
		echo test command: $(TEST_CMD_LINE); \
		$(TEST_CMD_LINE); \
		cd - > /dev/null; \
	)

valgrindtest: DEBUGGER_ARGS = --leak-check=full --show-leak-kinds=all --trace-children=no --track-origins=yes
valgrindtest: DEBUGGER = valgrind
valgrindtest: test

vtest: valgrindtest

gdbtest: DEBUGGER_ARGS = -ex=r --args
gdbtest: DEBUGGER = gdb
gdbtest: test

gtest: gdbtest

santest: asan = 1
santest: test

stest: santest

.PHONY: test vtest gtest stest valgrindtest gdbtest santest

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

ifeq ($(MODULES_NAME),all)
	TEST_EXEC = $(wildcard $(TEST_BIN_PATH)/*)
	MODULES_NAME =
endif

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

ifneq ($(repeat), )
	TEST_ARGS += --gtest_repeat="$(repeat)"
endif

ifneq ($(MODULES_NAME), )
test: modules = $(MODULES_NAME)
endif

$(MODULES_NAME):
$(TEST_NAME):
endif #test

#########
# Conan (extlibs)
#########

ifeq ($(word 1, $(MAKECMDGOALS)), dep)

ifeq (, $(shell which conan))
$(error "Makefile: no python-conan detected - it is needed to get dependencies for the project.")
endif

.PHONY: dep

dep: intro
	$(call log_info,makefile,starting conan with command: $(CONAN_INSTALL))
	@env test=$(test) verbose=$(verbose) modules=$(modules) lua=$(lua) py=$(py) libs=$(libs) $(CONAN_INSTALL)

# make dep mod MODULE
ifeq ($(word 2, $(MAKECMDGOALS)), mod)
MODULES_NAME = $(word 3, $(MAKECMDGOALS))$(m)
dep: modules = $(MODULES_NAME)
mod:
$(MODULES_NAME):
endif

# make dep mod MODULE
ifeq ($(word 2, $(MAKECMDGOALS)), lib)
LIBS_NAME = $(word 3, $(MAKECMDGOALS))
dep: modules = NONE
dep: libs = $(LIBS_NAME)
mod:
$(LIBS_NAME):
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

##############
# Distribution
##############

dist: dist = 1
dist: build

##########
# Serve
##########

serve:
	python3 -m http.server -d $(BIN_PATH) 3000

##########
# Cleanup
##########

clean:
	@$(call log_info,makefile,removing compilation build)
	rm -rf $(LIB_PATH)
	rm -rf $(INCLUDE_PATH)
	rm -rf $(TEST_PATH)
	rm -rf $(OBJ_PATH)
	rm -rf $(BIN_PATH)
	rm -rf $(RES_PATH)

clean_dep:
	@$(call log_info,makefile,removing dependencies)
	rm -rf $(CONAN_PATH) $(EXTLIB_PATH)

clean_dist:
	@$(call log_info,makefile,removing distribution)
	rm -rf $(DIST_PATH)

bclean:
	@$(call log_info,makefile,removing build)
	rm -rf $(BUILD_ENTRY_PATH) $(DIST_PATH)

fclean: bclean
	@$(call log_info,makefile,removing remaining files)
	find . -name "*vgcore*" -type f -exec rm -f {} \;
	find . -name "*.ini" -type f -exec rm -f {} \;
	find . -maxdepth 1 -name "*.scons*" -type d -exec rm -rf {} \;

### Makefile
.PHONY: install build verbose dist fclean clean clean_dist clean_dep

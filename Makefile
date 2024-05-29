PYTHON_BIN := python3
ifeq (, $(shell which $(PYTHON_BIN)))
PYTHON_BIN := python
endif

ifeq (, $(shell which $(PYTHON_BIN)))
$(error "Makefile: no python detected - it is needed to build the project.")
endif

ifeq (, $(shell which scons))
$(error "Makefile: no python-scons detected - it is needed to build the project.")
endif

OS := $(shell uname)

ifeq ($(OS),Linux)
SHELL := /bin/bash
GREP := /bin/grep
else
SHELL := /bin/sh
GREP := /usr/bin/grep
endif

ifdef MAKE_DEBUG
SHELL += -x
endif

ifndef MAKE_VERBOSE
QUIET := @
endif

MAKEARG_1 := $(word 1, $(MAKECMDGOALS))
MAKEARG_2 := $(word 2, $(MAKECMDGOALS))
MAKEARG_3 := $(word 3, $(MAKECMDGOALS))
MAKEARG_4 := $(word 4, $(MAKECMDGOALS))

HERE := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
APP_NAME := $(shell basename $(HERE))

##############
# Builder env
##############

BUILD_TOOLS := $(HERE)/site_scons

BUILD_SCRIPTS := $(BUILD_TOOLS)/scripts
BUILDER := $(BUILD_SCRIPTS)/builder.py

BUILDER_RESP := $(shell arch=$(arch) mode=$(mode) platform=$(platform) compiler=$(compiler) $(PYTHON_BIN) $(BUILDER) all)

PLATFORM := $(word 1, $(BUILDER_RESP))
ifeq ($(PLATFORM),)
$(error "Makefile: platform not found - cannot find build path")
endif

ARCH := $(word 2, $(BUILDER_RESP))
ifeq ($(ARCH),)
$(error "Makefile: architecture not found - cannot find build path")
endif

COMPILE_MODE := $(word 3, $(BUILDER_RESP))
ifeq ($(COMPILE_MODE),)
$(error "Makefile: compilation mode not found - cannot find build path")
endif

COMPILER := $(word 4, $(BUILDER_RESP))
ANDROID := $(word 5, $(BUILDER_RESP))
BUILD_PATH := $(word 6, $(BUILDER_RESP))

##########
# Paths
##########

BUILD_ENTRY_PATH := $(HERE)/build
EXTLIB_PATH := $(BUILD_PATH)/extlib
EXTLIB_LIB_PATH := $(EXTLIB_PATH)/lib
LIB_PATH := $(BUILD_PATH)/lib
INCLUDE_PATH := $(BUILD_PATH)/include
TEST_PATH := $(BUILD_PATH)/test
TEST_BIN_PATH := $(TEST_PATH)/bin
BIN_PATH := $(BUILD_PATH)/bin
OBJ_PATH := $(BUILD_PATH)/obj
ETC_PATH := $(BUILD_PATH)/etc
DEMO_PATH := $(BUILD_PATH)/demo
SHARE_PATH := $(BUILD_PATH)/share
DIST_PATH := $(HERE)/dist

####################
# Makefile includes
####################

MAKEFILE_TOOLS := $(BUILD_TOOLS)/makefile

include $(MAKEFILE_TOOLS)/logger.mk
include $(MAKEFILE_TOOLS)/utils.mk
include $(MAKEFILE_TOOLS)/templates.mk

BUILD_TOOLS_ADDON := $(BUILD_TOOLS)/addon
MAKEFILE_TOOLS_ADDON := $(BUILD_TOOLS_ADDON)/makefile

ifneq ($(wildcard $(MAKEFILE_TOOLS_ADDON)/addon.mk),)
include $(MAKEFILE_TOOLS_ADDON)/addon.mk
endif

############
# Scons env
############

ifeq ($(j),)
	j := $$[ $(UTILS_LOGICAL_CORE_NUMBER) - 2 ]
endif
SCONS_BUILD_CMD = $(SCONS_PREFIX) scons -Q -j$(j) $(SCONS_ARGS)

############
# Conan env
############

CONAN_PATH := $(BUILD_PATH)/conan
CONAN_PROFILES_PATH := $(BUILD_TOOLS)/conan_profiles
CONAN_PROFILE := $(CONAN_PROFILES_PATH)/$(ARCH)/$(COMPILER).txt
CONAN_PROFILE_ARG =
CONAN_INSTALL = conan install $(HERE) -if $(CONAN_PATH) $(CONAN_INSTALL_PATH) $(CONAN_PROFILE_ARG) $(CONAN_ARGS)
# checking platform env to select a conan profile
ifneq ("$(wildcard $(CONAN_PROFILE))", "")
	CONAN_PROFILE_ARG = --profile $(CONAN_PROFILE)
endif

#########
# Exports
#########

export APP_NAME
export TEST_PATH
export BUILD_PATH
export LIB_PATH
export BIN_PATH
export ETC_PATH
export SHARE_PATH
export EXTLIB_PATH
export EXTLIB_LIB_PATH

##########
# Targets
##########

.PHONY: all help

all: build

help:
	$(call mk_log_info,makefile,list all targets: make list)
	$(call mk_log_info,makefile,build modules: make)
	$(call mk_log_info,makefile,get dependencies: make dep)
	$(call mk_log_info,makefile,build and run tests: make test)
	$(call mk_log_info,makefile,build specific modules: modules=<comma_separated_modules>)
	$(call mk_log_info,makefile,build with address sanatizer: asan=1)
	$(call mk_log_info,makefile,build with static libs: static=1)
	$(call mk_log_info,makefile,build specific release: mode=debug|release)
	$(call mk_log_info,makefile,cross build for windows: platform=win)
	$(call mk_log_info,makefile,distribute app: dist=tar|apt|pacman)
	$(call mk_log_info,makefile,build with clang: compiler=clang)
	$(call mk_log_info,makefile,build with emscripten: compiler=em)
	$(QUIET) echo > /dev/null

######################
# Build infos
######################

.PHONY: info # print build infos

info:
	$(call mk_log_info,makefile,project: $(APP_NAME))
	$(call mk_log_info,makefile,platform = $(PLATFORM))
	$(call mk_log_info,makefile,compiler = $(COMPILER))
	$(call mk_log_info,makefile,arch = $(ARCH))
	$(call mk_log_info,makefile,mode = $(COMPILE_MODE))
	$(call mk_log_info,makefile,logical cores = $(UTILS_LOGICAL_CORE_NUMBER) ($(j) used))
ifeq ($(ANDROID), true)
	$(call mk_log_warning,makefile,android detected)
endif
	$(call mk_log_info,makefile,build: $(BUILD_PATH))
	$(QUIET) echo > /dev/null

######################
# List Makefile rules
######################

.PHONY: list # Generate list of targets with descriptions

list:
	$(QUIET) echo "Makefile targets"
	$(QUIET) - $(foreach MAKEFILE_PATH, $(MAKEFILE_LIST), \
		echo ""; \
		echo "$(MAKEFILE_PATH):"; \
		grep '^.PHONY: .* \#' $(MAKEFILE_PATH) | sed 's/\.PHONY: \(.*\) \# \(.*\)/  \1: \2/' | expand -t20; \
	)

##################
# Scons (builder)
##################

.PHONY: build # Run scons builder ([mod <comma_separated_modules>])

build:
	$(QUIET) cd $(HERE)
	$(eval BUILD_CMD_LINE = \
			modules=$(modules) test=$(test) dist=$(dist) mode=$(mode) asan=$(asan) verbose=$(verbose) pkgdep=$(pkgdep))
	$(QUIET) $(call echo_log_info,makefile,starting build with command: '$(SCONS_BUILD_CMD) $(BUILD_CMD_LINE)')
	$(QUIET) $(BUILD_CMD_LINE) $(SCONS_BUILD_CMD)

.PHONY: build_debug # Run scons builder with scons debug

build_debug: SCONS_ARGS = --debug=count,duplicate,explain,findlibs,includes,memoizer,memory,objects,prepare,presub,stacktrace,time
build_debug: SCONS_PREFIX = time
build_debug: build

.PHONY: build_dry # Run scons builder with expected build

build_dry: SCONS_ARGS = --dry-run
build_dry: build

# make mod MODULE
ifeq ($(MAKEARG_1), mod)
.PHONY: mod
MODULES_NAME := $(MAKEARG_2),$(m)
mod: modules = $(MODULES_NAME)
mod: build
endif # module

########
# Test
########

.PHONY: test # Build modules, tests and runs tests ([comma_separated_modules|all|ls] [filter] [repeat=x])
.PHONY: stest san_test # Build and run tests with address sanatizer and runs tests
.PHONY: itest nointeract_test # Build and run tests with stdin closed
.PHONY: vtest valgrind_test # Build and run tests with valgrind debugger
.PHONY: gtest gdb_test # Build and run tests with gdb debugger
.PHONY: ttest strace_test # Build and run tests with strace

# find string 'test' in target
ifneq ($(findstring test,$(MAKEARG_1)), )

TEST_EXEC := $(wildcard $(TEST_BIN_PATH)/*)
TEST_DEFAULT_ARGS =
TEST_ARGS =
DEBUGGER_ARGS =

# app_module -> module
get_module_name = $(word 2, $(subst _, , $(notdir $1)))

test: test = 1
test: build
	$(QUIET) sh $(MAKEFILE_TOOLS)/scripts/generate_test_env.sh
	$(call mk_log_info,makefile,starting tests in build: $(TEST_PATH))
	$(QUIET) - $(foreach TEST_BIN, $(TEST_EXEC), \
		$(eval TEST_CMD_LINE = \
			env $(DEBUGGER) $(DEBUGGER_ARGS) $(TEST_BIN) $(TEST_DEFAULT_ARGS) $(TEST_ARGS)\
		) \
		$(eval TEST_MODULE_NAME = $(call get_module_name, $(TEST_BIN))) \
		cd $(HERE)/$(TEST_MODULE_NAME); \
		echo "" ;\
		echo "###################################################################################" ; \
		echo "# testing module: $(TEST_MODULE_NAME)" ; \
		echo "# test command: $(TEST_CMD_LINE)" ; \
		echo "###################################################################################" ; \
		echo "" ;\
		$(TEST_CMD_LINE); \
		cd - > /dev/null; \
	)

nointeract_test: TEST_DEFAULT_ARGS += 0>&-
nointeract_test: test
itest: nointeract_test

valgrind_test: DEBUGGER_ARGS = --leak-check=full --show-leak-kinds=all --trace-children=no --track-origins=yes
valgrind_test: DEBUGGER = valgrind
valgrind_test: test
vtest: valgrind_test

gdb_test: DEBUGGER_ARGS = -ex=r --args
gdb_test: DEBUGGER = gdb
gdb_test: test
gtest: gdb_test

san_test: asan = 1
san_test: test
stest: san_test

strace_test: DEBUGGER = strace
strace_test: test
ttest: strace_test

# handles:
#	make test
#	make test MODULE FILTER
#	make test MODULE ls
#	make test ls
#	make test t=FILTER
COMMA = ,
MODULES_NAME := $(MAKEARG_2),$(m)
MODULES_NAME_SPLIT := $(subst $(COMMA), ,$(MODULES_NAME))
TEST_NAME := $(MAKEARG_3)$(t)

# --gtest_color=(yes|no|auto) (Enable/disable colored output. The default is auto)
# --gtest_brief=1 (Only print test failures.)
# --gtest_recreate_environments_when_repeating
# --gtest_list_tests
# --gtest_also_run_disabled_tests
# --gtest_output=(json|xml)[:DIRECTORY_PATH/|:FILE_PATH]
# --gtest_stream_result_to=HOST:PORT
# --gtest_catch_exceptions=0
# --gtest_break_on_failure
TEST_DEFAULT_ARGS += --gtest_death_test_style=threadsafe --gtest_shuffle

ifeq ($(MODULES_NAME),all)
	TEST_EXEC := $(wildcard $(TEST_BIN_PATH)/*)
	MODULES_NAME =
endif

ifneq ($(MODULES_NAME), )
	TEST_EXEC := $(foreach var, $(MODULES_NAME_SPLIT), $(TEST_BIN_PATH)/$(APP_NAME)_$(var))
endif

# case: make test ls
ifeq ($(MODULES_NAME),ls)
ifeq ($(TEST_NAME), )
	TEST_EXEC := $(wildcard $(TEST_BIN_PATH)/*)
	TEST_NAME = ls
	MODULES_NAME =
endif
endif

# ls to list tests, else filter tests
ifeq ($(TEST_NAME),ls)
	TEST_DEFAULT_ARGS += --gtest_list_tests
else
	TEST_DEFAULT_ARGS += --gtest_filter="*$(TEST_NAME)*"
endif

ifneq ($(repeat), )
	TEST_DEFAULT_ARGS += --gtest_repeat="$(repeat)" --gtest_throw_on_failure
endif

ifneq ($(MODULES_NAME), )
test: modules = $(MODULES_NAME)
endif

$(MODULES_NAME):
	$(QUIET) echo > /dev/null

$(TEST_NAME):
	$(QUIET) echo > /dev/null

endif #test

##################
# Extlibs
##################

.PHONY: pkgdep # List dependencies for registered package manager

ifeq ($(MAKEARG_1), pkgdep)

PKG_MANAGER_NAME := $(MAKEARG_2)

ifeq ($(PKG_MANAGER_NAME),)
$(error "Makefile: must provide a package manager name")
endif

pkgdep: pkgdep = $(PKG_MANAGER_NAME)
pkgdep: build

$(PKG_MANAGER_NAME):
	$(QUIET) echo > /dev/null

endif

##################
# Conan (extlibs)
##################

ifeq ($(MAKEARG_1), dep)

ifeq (, $(shell which conan))
$(error "Makefile: no python-conan detected - it is needed to get dependencies for the project.")
endif

.PHONY: dep # Run conan to get dependencies ([mod <comma_separated_modules>] [lib <libname>])

dep:
	$(call mk_log_info,makefile,starting conan with command: $(CONAN_INSTALL))
	$(QUIET) env test=$(test) verbose=$(verbose) modules=$(modules) libs=$(libs) $(CONAN_INSTALL)

# make dep mod MODULE
ifeq ($(MAKEARG_2), mod)
MODULES_NAME := $(MAKEARG_3),$(m)
dep: modules = $(MODULES_NAME)
mod:
	$(QUIET) echo > /dev/null
$(MODULES_NAME):
	$(QUIET) echo > /dev/null
endif

# make dep lib LIBNAME
ifeq ($(MAKEARG_2), lib)
LIBS_NAME := $(MAKEARG_3)
dep: modules = NONE
dep: libs = $(LIBS_NAME)
mod:
	$(QUIET) echo > /dev/null
$(LIBS_NAME):
	$(QUIET) echo > /dev/null
endif

# make dep test
ifeq ($(MAKEARG_2), test)
dep: modules = NONE
dep: test = 1
test:
	$(QUIET) echo > /dev/null
endif

# make dep build
ifeq ($(MAKEARG_2), build)
dep: CONAN_ARGS = --build
build:
	$(QUIET) echo > /dev/null
endif

endif # dep


.PHONY: checkdep_html # Run conan's HTML dependency tree

checkdep_html:
	$(QUIET) conan info . --graph=$(CONAN_PATH)/dep_tree.html

.PHONY: checkdep # Run conan's TXT dependency tree

checkdep:
	$(QUIET) conan info . --graph=$(CONAN_PATH)/dep_tree.txt && cat $(CONAN_PATH)/dep_tree.txt

###############
# Distribution
###############

.PHONY: dist  # Compile and package build (tar/apt/pacman)

# make dist
ifeq ($(MAKEARG_1), dist)

MODULES_NAME := $(m)

# make dist mod MODULE type
ifeq ($(MAKEARG_2), mod)
.PHONY: mod
MODULES_NAME := $(MAKEARG_3),$(m)
DIST_TYPE := $(MAKEARG_4)
mod:
	$(QUIET) echo > /dev/null
else
# make dist type
DIST_TYPE = $(MAKEARG_2)
endif

dist: dist = $(DIST_TYPE)
dist: mode = release
dist: modules = $(MODULES_NAME)
dist: build

$(MODULES_NAME):
	$(QUIET) echo > /dev/null

$(DIST_TYPE):
	$(QUIET) echo > /dev/null

endif # dist

##########
# Install
##########

ifeq ($(INSTALL_PREFIX),)
    INSTALL_PREFIX := /usr/local
endif

INSTALLED_FILES_DESTINATION := $(HERE)/.installed
INSTALL_LIB_DEST := $(INSTALL_DESTDIR)$(INSTALL_PREFIX)/lib$(INSTALL_LIBSUFFIX)
INSTALL_BIN_DEST := $(INSTALL_DESTDIR)$(INSTALL_PREFIX)/bin$(INSTALL_BINSUFFIX)
INSTALL_ETC_DEST := $(INSTALL_DESTDIR)/etc$(INSTALL_ETCSUFFIX)
INSTALL_INCLUDE_DEST := $(INSTALL_DESTDIR)$(INSTALL_PREFIX)/include$(INSTALL_INCLUDESUFFIX)
INSTALL_SHARE_DEST := $(INSTALL_DESTDIR)$(INSTALL_PREFIX)/share$(INSTALL_INCLUDESUFFIX)

.PHONY: confirm_install # List dirs to install and ask confirmation

confirm_install:
	$(QUIET) $(call mk_log_info,makefile,will install in: $(INSTALL_DESTDIR)$(INSTALL_PREFIX))
	$(QUIET) $(call mk_log_info,makefile,change install directory with: INSTALL_DESTDIR)
	$(QUIET) $(call mk_log_info,makefile,change default /usr/local with: INSTALL_PREFIX)
	$(QUIET) echo -n "Please confirm installation directory [y/N] " && read answer && [ $${answer:-N} = y ]

.PHONY: install # List dirs to install, ask confirmation and install on system and creates a file with path of all installed files

install: confirm_install
	$(QUIET) $(call mk_log_info,makefile,installed files can be found in: $(INSTALLED_FILES_DESTINATION))
	$(QUIET) echo "# install date: `date "+%Y-%m-%d %H:%M:%S"`" > $(INSTALLED_FILES_DESTINATION)
# install lib
	$(QUIET) $(call echo_log_info,makefile,installing librairies: ${LIB_PATH} -> $(INSTALL_LIB_DEST))
	$(QUIET) for path in `ls -A $(LIB_PATH) 2>/dev/null`; do \
		dest=$(INSTALL_LIB_DEST)/$$path; \
		install -D --compare --mode=755 "$(LIB_PATH)/$$path" "$$dest"; \
		echo "$$dest" >> $(INSTALLED_FILES_DESTINATION); \
	done
# install bin
	$(QUIET) $(call echo_log_info,makefile,installing binaries: ${BIN_PATH} -> $(INSTALL_BIN_DEST))
	$(QUIET) for path in `ls -A $(BIN_PATH) 2>/dev/null`; do \
		dest=$(INSTALL_BIN_DEST)/$$path; \
		install -D --compare --mode=755 "$(BIN_PATH)/$$path" "$$dest"; \
		echo "$$dest" >> $(INSTALLED_FILES_DESTINATION); \
	done
# getting all root directories of built resources for future removal
	$(QUIET) for path in `ls -A $(ETC_PATH) 2>/dev/null`; do \
		dest="$(INSTALL_ETC_DEST)/$$path"; \
		echo "$$dest" >> $(INSTALLED_FILES_DESTINATION); \
	done
# install etc
	$(QUIET) $(call echo_log_info,makefile,installing resources: ${ETC_PATH} -> $(INSTALL_ETC_DEST))
	$(QUIET) for path in `find $(ETC_PATH) -type f 2>/dev/null | sed "s|${ETC_PATH}/||g"`; do \
		dest="$(INSTALL_ETC_DEST)/$$path"; \
		install -D --compare --mode=744 "$(ETC_PATH)/$$path" "$$dest"; \
	done
# getting all root directories of built includes for future removal
	$(QUIET) for path in `ls -A $(INCLUDE_PATH) 2>/dev/null`; do \
		dest="$(INSTALL_INCLUDE_DEST)/$$path"; \
		echo "$$dest" >> $(INSTALLED_FILES_DESTINATION); \
	done
# install headers
	$(QUIET) $(call echo_log_info,makefile,installing headers: ${INCLUDE_PATH} -> $(INSTALL_INCLUDE_DEST))
	$(QUIET) for path in `find $(INCLUDE_PATH) -type f 2>/dev/null | sed "s|${INCLUDE_PATH}/||g"`; do \
		dest=$(INSTALL_INCLUDE_DEST)/$$path; \
		install -D --compare --mode=744 "$(INCLUDE_PATH)/$$path" "$$dest"; \
	done
# getting all root directories of built share for future removal
	$(QUIET) for path in `ls -A $(SHARE_PATH) 2>/dev/null`; do \
		dest="$(INSTALL_SHARE_DEST)/$$path"; \
		echo "$$dest" >> $(INSTALLED_FILES_DESTINATION); \
	done
# install shared
	$(QUIET) $(call echo_log_info,makefile,installing shared: ${SHARE_PATH} -> $(INSTALL_SHARE_DEST))
	$(QUIET) for path in `find $(SHARE_PATH) -type f 2>/dev/null | sed "s|${SHARE_PATH}/||g"`; do \
		dest=$(INSTALL_SHARE_DEST)/$$path; \
		install -D --compare --mode=744 "$(SHARE_PATH)/$$path" "$$dest"; \
	done

.PHONY: uninstall # Read file with path of all installed files, asks for confirmation and removes every files

uninstall:
	$(QUIET) $(call mk_log_info,makefile,reading files to remove from: $(INSTALLED_FILES_DESTINATION))
	$(eval INSTALLED_FILES = $(shell cat $(INSTALLED_FILES_DESTINATION) | tail -n +2))
	$(QUIET) echo $(INSTALLED_FILES) | tr ' ' '\n'
	$(QUIET) $(call echo_log_warning,makefile,those files will be removed)
	$(QUIET) echo -n "Please confirm files removal [y/N] " && read answer && [ $${answer:-N} = y ]
	rm -rf $(INSTALLED_FILES)
	rm -f $(INSTALLED_FILES_DESTINATION)

##########
# Serve
##########

.PHONY: serve # Serve a python http.server on port $(PORT) (default: 8000)

serve:
	$(PYTHON_BIN) -m http.server -d $(BIN_PATH) ${PORT}

##########
# Cleanup
##########

.PHONY: clean # Remove platform-arch/release build
.PHONY: clean_dep # Remove platform-arch dependencies
.PHONY: cclean # Remove platform-arch/release build and cache
.PHONY: clean_dist # Remove distribution
.PHONY: clean_cache # Remove build cache
.PHONY: clean_build # Remove all builds and dependencies
.PHONY: fclean # Remove everything

clean:
	$(QUIET) $(call mk_log_info,makefile,removing compilation build)
	rm -rf $(OBJ_PATH)
	rm -rf $(INCLUDE_PATH)
	rm -rf $(LIB_PATH)
	rm -rf $(ETC_PATH)
	rm -rf $(SHARE_PATH)
	rm -rf $(BIN_PATH)
	rm -rf $(TEST_PATH)
	rm -rf $(DEMO_PATH)
	rm $(BUILD_PATH)/compile_commands.json

clean_dep:
	$(QUIET) $(call mk_log_info,makefile,removing dependencies)
	rm -rf $(CONAN_PATH) $(EXTLIB_PATH)

clean_dist:
	$(QUIET) $(call mk_log_info,makefile,removing distribution)
	rm -rf $(DIST_PATH)

clean_cache:
	$(QUIET) $(call mk_log_info,makefile,removing cache)
	rm -rf $(HERE)/.scons_cache

cclean: clean_cache clean

clean_build:
	$(QUIET) $(call mk_log_info,makefile,removing build)
	rm -rf $(BUILD_ENTRY_PATH)

fclean: clean_build clean_dist clean_cache
	$(QUIET) $(call mk_log_info,makefile,removing remaining files)
	$(QUIET) rm -f $(HERE)/.sconsign.dblite

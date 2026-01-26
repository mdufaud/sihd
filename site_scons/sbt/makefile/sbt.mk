PYTHON_BIN := python3
ifeq (, $(shell which $(PYTHON_BIN) 2>/dev/null))
PYTHON_BIN := python
endif

PIP_BIN := pip3
ifeq (, $(shell which $(PIP_BIN) 2>/dev/null))
PIP_BIN := pip
endif

ifeq (, $(shell which $(PYTHON_BIN) 2>/dev/null))
$(error "Makefile: no python detected - it is needed to build the project.")
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

PROJECT_ROOT_PATH := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

APP_NAME := $(shell basename $(PROJECT_ROOT_PATH))

##############
# Builder env
##############

BUILD_TOOLS := $(PROJECT_ROOT_PATH)/site_scons
SBT_PATH := $(BUILD_TOOLS)/sbt

SBT_CLI := $(SBT_PATH)/cli_helper.py
SBT_RESP := $(shell machine=$(machine) arch=$(arch) mode=$(mode) platform=$(platform) compiler=$(compiler) $(PYTHON_BIN) $(SBT_CLI) all)

PLATFORM := $(word 1, $(SBT_RESP))
ifeq ($(PLATFORM),)
$(error "Makefile: platform not found - cannot find build path")
endif

MACHINE := $(word 2, $(SBT_RESP))
ifeq ($(MACHINE),)
$(error "Makefile: machine not found - cannot find build path")
endif

COMPILE_MODE := $(word 3, $(SBT_RESP))
ifeq ($(COMPILE_MODE),)
$(error "Makefile: compilation mode not found - cannot find build path")
endif

ARCH := $(word 4, $(SBT_RESP))

COMPILER := $(word 5, $(SBT_RESP))
GNU_TRIPLET := $(word 6, $(SBT_RESP))
ANDROID := $(word 7, $(SBT_RESP))
BUILD_PATH := $(word 8, $(SBT_RESP))

##########
# Paths
##########

BUILD_ENTRY_PATH := $(PROJECT_ROOT_PATH)/build
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
DIST_PATH := $(PROJECT_ROOT_PATH)/dist

####################
# Makefile includes
####################

MAKEFILE_TOOLS := $(SBT_PATH)/makefile

include $(MAKEFILE_TOOLS)/logger.mk
include $(MAKEFILE_TOOLS)/utils.mk
include $(MAKEFILE_TOOLS)/templates.mk

BUILD_TOOLS_ADDON := $(BUILD_TOOLS)/addon
MAKEFILE_TOOLS_ADDON := $(BUILD_TOOLS_ADDON)/makefile

ifneq ($(wildcard $(MAKEFILE_TOOLS_ADDON)/addon.mk),)
include $(MAKEFILE_TOOLS_ADDON)/addon.mk
endif

############
# Venv
############

VENV_PATH := $(PROJECT_ROOT_PATH)/.venv

############
# Scons env
############

ifeq ($(j),)
	j := $$[ $(UTILS_LOGICAL_CORE_NUMBER) - 1 ]
endif
SCONS_BUILD_CMD = $(SCONS_PREFIX) scons -Q -j$(j) $(SCONS_ARGS)

############
# VCPKG
############

VCPKG_PATH := $(PROJECT_ROOT_PATH)/.vcpkg
VCPKG_BIN := $(VCPKG_PATH)/vcpkg
VCPKG_SCRIPT_PATH := $(SBT_PATH)/vcpkg.py
VCPKG_ACTION := fetch

VCPKG_SCRIPT = $(PYTHON_BIN) $(VCPKG_SCRIPT_PATH) $(VCPKG_ACTION)

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
	$(call mk_log_info,makefile,list all targets: make targets)
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
	$(call mk_log_info,makefile,machine = $(MACHINE))
	$(call mk_log_info,makefile,arch = $(ARCH))
	$(call mk_log_info,makefile,gnu_triplet = $(GNU_TRIPLET))
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

.PHONY: targets # Generate list of targets with descriptions

targets:
	$(QUIET) echo "Makefile targets"
	$(QUIET) - $(foreach MAKEFILE_PATH, $(MAKEFILE_LIST), \
		echo ""; \
		echo "$(MAKEFILE_PATH):"; \
		grep '^.PHONY: .* \#' $(MAKEFILE_PATH) | sed 's/\.PHONY: \(.*\) \# \(.*\)/  \1: \2/' | expand -t20; \
	)

##################
# Venv
##################

.PHONY: venv # Go into a python virtual environnement

venv:
ifeq ($(VIRTUAL_ENV), )
	$(PYTHON_BIN) -m venv $(VENV_PATH)
	$(QUIET) echo "Start with:"
	$(QUIET) echo "$> source `basename $(VENV_PATH)`/bin/activate"
	$(QUIET) echo "End with:"
	$(QUIET) echo "$> deactivate"
endif

.PHONY: setup # Install scons in venv

setup: venv
	(source $(VENV_PATH)/bin/activate && $(PIP_BIN) install scons)

##################
# Scons (builder)
##################

.PHONY: build # Run scons builder ([mod <comma_separated_modules>])

build:
	$(QUIET) cd $(PROJECT_ROOT_PATH)
	$(eval BUILD_CMD_LINE = \
			modules=$(modules) \
			test=$(test) \
			dist=$(dist) \
			mode=$(mode) \
			asan=$(asan) \
			verbose=$(verbose) \
			pkgdep=$(pkgdep) \
			demo=$(demo) \
	)
	$(QUIET) $(call echo_log_info,makefile,starting build with command: '$(SCONS_BUILD_CMD) $(BUILD_CMD_LINE)')
	$(QUIET) $(SCONS_BUILD_CMD) $(BUILD_CMD_LINE)

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
.PHONY: ltest valgrind_leak_test # Build and run tests with valgrind leak checking debugger
.PHONY: gtest gdb_test # Build and run tests with gdb debugger
.PHONY: ttest strace_test # Build and run tests with strace

# find string 'test' in target
ifneq ($(findstring test,$(MAKEARG_1)), )

# --gtest_color=(yes|no|auto) (Enable/disable colored output. The default is auto)
# --gtest_brief=1 (Only print test failures.)
# --gtest_recreate_environments_when_repeating
# --gtest_list_tests
# --gtest_also_run_disabled_tests
# --gtest_output=(json|xml)[:DIRECTORY_PATH/|:FILE_PATH]
# --gtest_stream_result_to=HOST:PORT
# --gtest_catch_exceptions=0
# --gtest_break_on_failure
TEST_ARGS :=
TEST_ACTION := test
TEST_SCRIPT_ARGS :=
DEBUGGER :=
DEBUGGER_ARGS :=

# handles:
#	make test
#	make test MODULE FILTER
#	make test MODULE ls
#	make test ls
#	make test t=FILTER
MODULES_NAME := $(MAKEARG_2),$(m)
TEST_NAME_FILTER := $(MAKEARG_3)$(t)

ifeq ($(MODULES_NAME),all)
	MODULES_NAME :=
endif

# case: make test ls
ifeq ($(MAKEARG_2),ls)
ifeq ($(TEST_NAME_FILTER), )
	TEST_ACTION := list
	MODULES_NAME :=
endif
endif

# case: make test MODULE ls
ifeq ($(MAKEARG_3),ls)
	TEST_ACTION := list
endif

ifeq ($(TEST_ACTION),list)
ls:
	$(QUIET) echo > /dev/null
endif

ifneq ($(MODULES_NAME), )
test: modules = $(MODULES_NAME)
endif

COMMA := ,
MODULES_NAME_SPLIT := $(subst $(COMMA), ,$(MODULES_NAME))

test: test = 1
test: build
	$(call mk_log_info,makefile,starting tests in build: $(TEST_PATH))
	$(QUIET) bash $(MAKEFILE_TOOLS)/scripts/generate_test_env.sh
	$(QUIET) env TEST_BIN_PATH="$(TEST_BIN_PATH)" \
					PROJECT_ROOT_PATH="$(PROJECT_ROOT_PATH)" \
					bash $(MAKEFILE_TOOLS)/scripts/generate_test_executor.sh
	$(QUIET) env DEBUGGER="$(DEBUGGER)" \
					DEBUGGER_ARGS="$(DEBUGGER_ARGS)" \
					TEST_ARGS="$(TEST_ARGS)" \
					REPEAT="$(repeat)" \
					ASAN_OPTIONS="$(ASAN_OPTIONS)" \
					bash $(TEST_PATH)/execute_tests.sh "$(TEST_ACTION)" "$(MODULES_NAME_SPLIT)" "$(TEST_NAME_FILTER)" $(TEST_SCRIPT_ARGS)

nointeract_test: TEST_SCRIPT_ARGS += 0>&-
nointeract_test: test
itest: nointeract_test

valgrind_test: DEBUGGER_ARGS = --trace-children=no --track-origins=no --show-leak-kinds=definite
valgrind_test: DEBUGGER = valgrind
valgrind_test: test
vtest: valgrind_test

valgrind_leak_test: DEBUGGER_ARGS = --leak-check=full --show-leak-kinds=all --trace-children=no --track-origins=yes
valgrind_leak_test: DEBUGGER = valgrind
valgrind_leak_test: test
ltest: valgrind_leak_test

gdb_test: DEBUGGER_ARGS = --args
gdb_test: DEBUGGER = gdb
gdb_test: test
gtest: gdb_test



san_test: ASAN_OPTIONS="detect_leaks=1:halt_on_error=0:strict_init_order=1:detect_odr_violation=1:detect_stack_use_after_return=1:detect_container_overflow=1:alloc_dealloc_mismatch=1:dectect_invalid_pointers_pairs=2:verbosity=0:atexit=1:check_initialization_order=1"
san_test: asan = 1
san_test: test
stest: san_test

strace_test: DEBUGGER = strace
strace_test: test
ttest: strace_test

$(MODULES_NAME):
	$(QUIET) echo > /dev/null

$(TEST_NAME_FILTER):
	$(QUIET) echo > /dev/null

endif # test

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

endif # pkgdep

##################
# Demo
##################

.PHONY: demo

ifeq ($(MAKEARG_1), demo)

MODULES_NAME := $(MAKEARG_2),$(m)
demo: modules = $(MODULES_NAME)
demo: demo = 1
demo: build

endif


##################
# VCPKG (extlibs)
##################

.PHONY: vcpkg_deploy

vcpkg_deploy:
	$(QUIET) if [ ! -d "$(VCPKG_PATH)" ]; then git clone https://github.com/microsoft/vcpkg $(VCPKG_PATH); fi
	$(QUIET) if [ ! -f "$(VCPKG_BIN)" ]; then $(VCPKG_PATH)/bootstrap-vcpkg.sh -disableMetrics; fi

.PHONY: dep # Run vcpkg to get dependencies

dep: vcpkg_deploy
	$(QUIET) env test=$(test) verbose=$(verbose) modules=$(modules) $(VCPKG_SCRIPT)

ifeq ($(MAKEARG_1), dep)

ifeq ($(MAKEARG_2), mod)
VCPKG_ACTION := fetch
MODULES_NAME := $(MAKEARG_3),$(m)
dep: modules = $(MODULES_NAME)
mod:
	$(QUIET) echo > /dev/null
endif

ifeq ($(MAKEARG_2), demo)
VCPKG_ACTION := fetch
MODULES_NAME := $(MAKEARG_3),$(m)
dep: demo = 1
dep: modules = $(MODULES_NAME)
demo:
	$(QUIET) echo > /dev/null
endif

ifeq ($(MAKEARG_2), test)
VCPKG_ACTION := fetch
MODULES_NAME := $(MAKEARG_3),$(m)
dep: test = 1
dep: modules = $(MODULES_NAME)
test:
	$(QUIET) echo > /dev/null
endif

ifeq ($(MAKEARG_2), list)
VCPKG_ACTION := list
list: dep
	$(QUIET) echo > /dev/null
endif

ifeq ($(MAKEARG_2), tree)
VCPKG_ACTION := tree
tree: dep
	$(QUIET) echo > /dev/null
endif

ifeq ($(MAKEARG_2), install)
VCPKG_ACTION :=
install: vcpkg_deploy
	$(VCPKG_BIN) install $(MAKEARG_3)
endif

ifeq ($(MAKEARG_2), search)
VCPKG_ACTION :=
search: vcpkg_deploy
	$(VCPKG_BIN) search $(MAKEARG_3) --x-full-desc
endif

ifeq ($(MAKEARG_2), update)
VCPKG_ACTION :=
update: vcpkg_deploy
	cd $(VCPKG_PATH) && git pull && $(VCPKG_BIN) update
endif


endif # dep

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

ifeq ($(MAKEARG_1), install)

ifeq ($(INSTALL_PREFIX),)
    INSTALL_PREFIX := /usr/local
endif

INSTALLED_FILES_DESTINATION := $(PROJECT_ROOT_PATH)/.installed
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

endif # install

ifeq ($(MAKEARG_1), uninstall)

.PHONY: uninstall # Read file with path of all installed files, asks for confirmation and removes every files

uninstall:
	$(QUIET) $(call mk_log_info,makefile,reading files to remove from: $(INSTALLED_FILES_DESTINATION))
	$(eval INSTALLED_FILES = $(shell cat $(INSTALLED_FILES_DESTINATION) | tail -n +2))
	$(QUIET) echo $(INSTALLED_FILES) | tr ' ' '\n'
	$(QUIET) $(call echo_log_warning,makefile,those files will be removed)
	$(QUIET) echo -n "Please confirm files removal [y/N] " && read answer && [ $${answer:-N} = y ]
	rm -rf $(INSTALLED_FILES)
	rm -f $(INSTALLED_FILES_DESTINATION)

endif # uninstall


##########
# Serve
##########

.PHONY: serve_bin # Serve a python http.server on port $(PORT) (default: 8000)
.PHONY: serve_demo # Serve a python http.server on port $(PORT) (default: 8000)

# TODO add
# Cross-Origin-Opener-Policy: same-origin
# Cross-Origin-Embedder-Policy: require-corp

serve_bin:
	$(PYTHON_BIN) -m http.server -d $(BIN_PATH) ${PORT}

serve_demo:
	$(PYTHON_BIN) -m http.server -d $(DEMO_PATH) ${PORT}

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
	rm -f $(BUILD_PATH)/compile_commands.json

clean_dep:
	$(QUIET) $(call mk_log_info,makefile,removing dependencies)
	rm -rf $(BUILD_PATH)/vcpkg
	rm -f $(EXTLIB_PATH)
	rm -rf $(VCPKG_PATH)/packages

clean_dist:
	$(QUIET) $(call mk_log_info,makefile,removing distribution)
	rm -rf $(DIST_PATH)

clean_cache:
	$(QUIET) $(call mk_log_info,makefile,removing cache)
	rm -rf $(PROJECT_ROOT_PATH)/.scons_cache

cclean: clean_cache clean

clean_build:
	$(QUIET) $(call mk_log_info,makefile,removing build)
	rm -rf $(BUILD_ENTRY_PATH)

fclean: clean_build clean_dist clean_cache clean_dep
	$(QUIET) $(call mk_log_info,makefile,removing remaining files)
	$(QUIET) rm -f $(PROJECT_ROOT_PATH)/.sconsign.dblite

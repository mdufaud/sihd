UTILS_LOGICAL_CORE_NUMBER = $(shell nproc --all)
ifeq ($(UTILS_LOGICAL_CORE_NUMBER),)
	LOGICAL_CORE_NUMBER = $(shell getconf _NPROCESSORS_ONLN)
endif
ifeq ($(UTILS_LOGICAL_CORE_NUMBER),)
	LOGICAL_CORE_NUMBER = $(shell lscpu | grep "CPU(s):" | awk '{print $2}')
endif
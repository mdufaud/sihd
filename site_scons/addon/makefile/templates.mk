MAKEFILE_ADDON_TEMPLATES := $(MAKEFILE_TOOLS_ADDON)/templates
MAKEFILE_ADDON_SCRIPTS := $(MAKEFILE_TOOLS_ADDON)/scripts

.PHONY: newdevice # Generate a sihd device (<module_name> <class_name>)

ifeq ($(word 1, $(MAKECMDGOALS)), newdevice)
MODULE_NAME := $(word 2, $(MAKECMDGOALS))$(m)
CLASS_NAME := $(word 3, $(MAKECMDGOALS))$(c)

newdevice:
	@bash $(MAKEFILE_ADDON_SCRIPTS)/make_device_class.sh $(APP_NAME) $(MODULE_NAME) $(CLASS_NAME)

# for no 'no rules to make...'
$(MODULE_NAME):
	$(QUIET) echo > /dev/null

$(CLASS_NAME):
	$(QUIET) echo > /dev/null

endif

.PHONY: newnamedclass # Generate a sihd named class (<module_name> <class_name>)

ifeq ($(word 1, $(MAKECMDGOALS)), newnamedclass)
MODULE_NAME := $(word 2, $(MAKECMDGOALS))$(m)
CLASS_NAME := $(word 3, $(MAKECMDGOALS))$(c)

newnamedclass:
	@bash $(MAKEFILE_ADDON_SCRIPTS)/make_named_class.sh $(APP_NAME) $(MODULE_NAME) $(CLASS_NAME)

$(MODULE_NAME):
	$(QUIET) echo > /dev/null

$(CLASS_NAME):
	$(QUIET) echo > /dev/null

endif

.PHONY: newsihdtest # Generate a sihd test (<module_name> <test_name>)

ifeq ($(word 1, $(MAKECMDGOALS)), newsihdtest)
MODULE_NAME := $(word 2, $(MAKECMDGOALS))$(m)
TEST_NAME := $(word 3, $(MAKECMDGOALS))$(t)

newsihdtest:
	@bash $(MAKEFILE_ADDON_SCRIPTS)/make_sihd_test.sh $(APP_NAME) $(MODULE_NAME) $(TEST_NAME)

endif

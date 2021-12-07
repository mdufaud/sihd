MAKEFILE_EXTRA_TEMPLATES = $(MAKEFILE_EXTRA)/templates
MAKEFILE_EXTRA_SCRIPTS = $(MAKEFILE_EXTRA)/scripts

ifeq ($(word 1, $(MAKECMDGOALS)), newdevice)
MODULE_NAME=$(word 2, $(MAKECMDGOALS))$(m)
CLASS_NAME=$(word 3, $(MAKECMDGOALS))$(c)

newdevice:
	@bash $(MAKEFILE_EXTRA_SCRIPTS)/make_device_class.sh $(APP_NAME) $(MODULE_NAME) $(CLASS_NAME)

# for no 'no rules to make...'
$(MODULE_NAME):

# for no 'no rules to make...'
$(CLASS_NAME):

endif

ifeq ($(word 1, $(MAKECMDGOALS)), newnamedclass)
MODULE_NAME=$(word 2, $(MAKECMDGOALS))$(m)
CLASS_NAME=$(word 3, $(MAKECMDGOALS))$(c)

newnamedclass:
	@bash $(MAKEFILE_EXTRA_SCRIPTS)/make_named_class.sh $(APP_NAME) $(MODULE_NAME) $(CLASS_NAME)

# for no 'no rules to make...'
$(MODULE_NAME):

# for no 'no rules to make...'
$(CLASS_NAME):

endif

ifeq ($(word 1, $(MAKECMDGOALS)), newlogtest)
MODULE_NAME=$(word 2, $(MAKECMDGOALS))$(m)
TEST_NAME=$(word 3, $(MAKECMDGOALS))$(t)

newlogtest:
	@bash $(MAKEFILE_EXTRA_SCRIPTS)/make_log_test.sh $(APP_NAME) $(MODULE_NAME) $(TEST_NAME)

# for no 'no rules to make...'
$(MODULE_NAME):

# for no 'no rules to make...'
$(TEST_NAME):

endif
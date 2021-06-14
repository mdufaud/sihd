BUILD_TOOLS_TEMPLATES = $(BUILD_TOOLS)/templates
BUILD_TOOLS_SCRIPTS = $(BUILD_TOOLS)/scripts

ifeq ($(word 1, $(MAKECMDGOALS)), newmod)
MODULE_NAME=$(word 2, $(MAKECMDGOALS))$(m)

newmod:
	@bash $(BUILD_TOOLS_SCRIPTS)/make_module.sh $(APP_NAME) $(MODULE_NAME)

# for no 'no rules to make...'
$(MODULE_NAME):

endif

ifeq ($(word 1, $(MAKECMDGOALS)), newtest)
MODULE_NAME=$(word 2, $(MAKECMDGOALS))$(m)
TEST_NAME=$(word 3, $(MAKECMDGOALS))$(t)

newtest:
	@bash $(BUILD_TOOLS_SCRIPTS)/make_test.sh $(APP_NAME) $(MODULE_NAME) $(TEST_NAME)

# for no 'no rules to make...'
$(MODULE_NAME):

# for no 'no rules to make...'
$(TEST_NAME):

endif


ifeq ($(word 1, $(MAKECMDGOALS)), newclass)
MODULE_NAME=$(word 2, $(MAKECMDGOALS))$(m)
CLASS_NAME=$(word 3, $(MAKECMDGOALS))$(c)

newclass:
	@bash $(BUILD_TOOLS_SCRIPTS)/make_class.sh $(APP_NAME) $(MODULE_NAME) $(CLASS_NAME)

# for no 'no rules to make...'
$(MODULE_NAME):

# for no 'no rules to make...'
$(CLASS_NAME):

endif

ifeq ($(word 1, $(MAKECMDGOALS)), newnamedclass)
MODULE_NAME=$(word 2, $(MAKECMDGOALS))$(m)
CLASS_NAME=$(word 3, $(MAKECMDGOALS))$(c)

newnamedclass:
	@bash $(BUILD_TOOLS_SCRIPTS)/make_named_class.sh $(APP_NAME) $(MODULE_NAME) $(CLASS_NAME)

# for no 'no rules to make...'
$(MODULE_NAME):

# for no 'no rules to make...'
$(CLASS_NAME):

endif

ifeq ($(word 1, $(MAKECMDGOALS)), newinterface)
MODULE_NAME=$(word 2, $(MAKECMDGOALS))$(m)
CLASS_NAME=$(word 3, $(MAKECMDGOALS))$(c)

newinterface:
	@bash $(BUILD_TOOLS_SCRIPTS)/make_interface_file.sh $(APP_NAME) $(MODULE_NAME) $(CLASS_NAME)

# for no 'no rules to make...'
$(MODULE_NAME):

# for no 'no rules to make...'
$(CLASS_NAME):

endif
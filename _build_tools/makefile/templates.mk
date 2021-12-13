MAKEFILE_TOOLS_TEMPLATES = $(MAKEFILE_TOOLS)/templates
MAKEFILE_TOOLS_SCRIPTS = $(MAKEFILE_TOOLS)/scripts

export MAKEFILE_TOOLS_TEMPLATES
export MAKEFILE_TOOLS_SCRIPTS

ifeq ($(word 1, $(MAKECMDGOALS)), newmod)
MODULE_NAME=$(word 2, $(MAKECMDGOALS))$(m)

newmod:
	@bash $(MAKEFILE_TOOLS_SCRIPTS)/make_module.sh $(APP_NAME) $(MODULE_NAME)

# for no 'no rules to make...'
$(MODULE_NAME):

endif

ifeq ($(word 1, $(MAKECMDGOALS)), newtest)
MODULE_NAME=$(word 2, $(MAKECMDGOALS))$(m)
TEST_NAME=$(word 3, $(MAKECMDGOALS))$(t)

newtest:
	@bash $(MAKEFILE_TOOLS_SCRIPTS)/make_test.sh $(APP_NAME) $(MODULE_NAME) $(TEST_NAME)

# for no 'no rules to make...'
$(MODULE_NAME):

# for no 'no rules to make...'
$(TEST_NAME):

endif


ifeq ($(word 1, $(MAKECMDGOALS)), newclass)
MODULE_NAME=$(word 2, $(MAKECMDGOALS))$(m)
CLASS_NAME=$(word 3, $(MAKECMDGOALS))$(c)

newclass:
	@bash $(MAKEFILE_TOOLS_SCRIPTS)/make_class.sh $(APP_NAME) $(MODULE_NAME) $(CLASS_NAME)

# for no 'no rules to make...'
$(MODULE_NAME):

# for no 'no rules to make...'
$(CLASS_NAME):

endif

ifeq ($(word 1, $(MAKECMDGOALS)), newinterface)
MODULE_NAME=$(word 2, $(MAKECMDGOALS))$(m)
CLASS_NAME=$(word 3, $(MAKECMDGOALS))$(c)

newinterface:
	@bash $(MAKEFILE_TOOLS_SCRIPTS)/make_interface_file.sh $(APP_NAME) $(MODULE_NAME) $(CLASS_NAME)

# for no 'no rules to make...'
$(MODULE_NAME):

# for no 'no rules to make...'
$(CLASS_NAME):

endif
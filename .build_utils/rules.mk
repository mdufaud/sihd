ifeq ($(word 1, $(MAKECMDGOALS)), newmod)
MODULE_NAME=$(word 2, $(MAKECMDGOALS))$(m)

newmod:
	@bash $(BUILD_UTILS)/make_module.sh $(APPNAME) $(MODULE_NAME)

# for no 'no rules to make...'
$(MODULE_NAME):

endif

ifeq ($(word 1, $(MAKECMDGOALS)), newtest)
MODULE_NAME=$(word 2, $(MAKECMDGOALS))$(m)
TEST_NAME=$(word 3, $(MAKECMDGOALS))$(t)

newtest:
	@bash $(BUILD_UTILS)/make_test.sh $(APPNAME) $(MODULE_NAME) $(TEST_NAME)

# for no 'no rules to make...'
$(MODULE_NAME):

# for no 'no rules to make...'
$(TEST_NAME):

endif


ifeq ($(word 1, $(MAKECMDGOALS)), newclass)
MODULE_NAME=$(word 2, $(MAKECMDGOALS))$(m)
CLASS_NAME=$(word 3, $(MAKECMDGOALS))$(c)

newclass:
	@bash $(BUILD_UTILS)/make_class.sh $(APPNAME) $(MODULE_NAME) $(CLASS_NAME)

# for no 'no rules to make...'
$(MODULE_NAME):

# for no 'no rules to make...'
$(CLASS_NAME):

endif

ifeq ($(word 1, $(MAKECMDGOALS)), newnamedclass)
MODULE_NAME=$(word 2, $(MAKECMDGOALS))$(m)
CLASS_NAME=$(word 3, $(MAKECMDGOALS))$(c)

newnamedclass:
	@bash $(BUILD_UTILS)/make_named_class.sh $(APPNAME) $(MODULE_NAME) $(CLASS_NAME)

# for no 'no rules to make...'
$(MODULE_NAME):

# for no 'no rules to make...'
$(CLASS_NAME):

endif
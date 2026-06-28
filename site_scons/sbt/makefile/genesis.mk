##########
# Project
##########

ifeq ($(MAKEARG_1), newsbt)

PROJECT_PATH := $(MAKEARG_2)

newsbt:
ifeq ($(PROJECT_PATH),)
	$(error Usage: make newsbt <project_path>)
endif
	if [ -d "$(PROJECT_PATH)" ]; then echo "Error: directory already exists: $(PROJECT_PATH)"; exit 1; fi
	mkdir -p $(PROJECT_PATH)/site_scons
	cp -r $(SBT_PATH)/genesis_fs/* $(PROJECT_PATH)
	cp -r $(SBT_PATH)/ $(PROJECT_PATH)/site_scons/sbt
	echo "New SBT project created in: $(PROJECT_PATH)"

endif # newsbt

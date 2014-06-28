# Add target for creating the global resource file.
# If that file is not set (i.e. non-Windows), doesn't do anything.

ifeq ($(GLOBAL_RESOURCE_FILE_TARGET),)

GLOBAL_RESOURCE_FILE_TARGET:=1

ifneq ($(GLOBAL_RESOURCE_FILE),)

$(GLOBAL_RESOURCE_FILE): misc/UAC-asInvoker.rc misc/UAC-asInvoker.manifest
	$(PLATFORM_PACKAGES_DIR)/bin/windres --input misc/UAC-asInvoker.rc --output $(GLOBAL_RESOURCE_FILE) --output-format=coff

endif

endif

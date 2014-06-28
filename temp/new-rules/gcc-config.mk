# 

ifeq (,$(GCC_CONFIG_INCLUDED))

GCC_CONFIG_INCLUDED:=1

ifneq (1,$(ALL_RULES_INCLUDES))
$(error You need to include all-rules.mk before gcc-config.mk)
endif

# We need to know about cov-configure

# We need to save those variables, because this file is included in
# the middle of create-analysis-lib, and prog.mk will overwrite
# those variables
$(call save_var,NAME)
$(call save_var,SOURCE_DIR)
$(call save_var,SOURCES)

include $(RULES)/../build/cov-configure/prog.mk	

$(call restore_var,SOURCES)
$(call restore_var,SOURCE_DIR)
$(call restore_var,NAME)

ifeq ($(USE_HOST_COMPILER),1)
ifneq ($(CONFIGURE_HOST_GCC),1)
CONFIGURE_HOST_GCC:=@true
else
CONFIGURE_HOST_GCC:=$(COV_CONFIGURE) -co gcc -c $(GCC_CONFIG)
endif
else
CONFIGURE_HOST_GCC:=$(COV_CONFIGURE) -co gcc -c $(GCC_CONFIG)
endif

ifeq ($(HAS_CCACHE),1)
CONFIGURE_CCACHE:=$(COV_CONFIGURE) -co $(CCACHE) --comptype prefix -c $(GCC_CONFIG)
else
CONFIGURE_CCACHE:=@true
endif

ifeq ($(BUILD_FE_CLANG),1)
CONFIGURE_CLANG:=$(COV_CONFIGURE) -co $(CLANG_CC) --comptype clangcc \
  -c $(GCC_CONFIG)
else
CONFIGURE_CLANG:=@true
endif

TEST_CL:=$(FAKE_COMPILERS_BIN_DIR)/cl$(EXE_POSTFIX)

# If $(CC) requires any options for compilation, add them to config.
# Currently enabled only for Mac OS X 10.7 (Lion).
CONFIGURE_CC:=$(COV_CONFIGURE) -co $(CC) -c $(GCC_CONFIG)
ifneq ($(CC_REQUIRED_OPTS),)
CONFIGURE_CC+= -- $(CC_REQUIRED_OPTS)
endif

$(GCC_CONFIG): $(COV_CONFIGURE) | symlinks $(TEST_CL)
	rm -rf $(GCC_CONFIG_DIR)
	@# DR 18843 - Configure an MSVC compiler in order to test PCH
	@# DR 54362 - The forced cov-configure is expected to generate a couple
	@#     error messages that we tidy up for the build logs
	$(COV_CONFIGURE) -co $(TEST_CL) --force -c $@ 2>&1 | sed 's/Error: Native compiler is too permissive/(Expected) Fake compiler is too permissive/;s/Error generating configuration for compiler/(Expected) Normal configuration failed for fake compiler/'; exit $${PIPESTATUS[0]}
	$(CONFIGURE_CC)
	$(CONFIGURE_HOST_GCC)
	$(CONFIGURE_CLANG)
	$(CONFIGURE_CCACHE)

.PHONY:gcc-config

gcc-config: $(GCC_CONFIG)

include $(RULES)/../symlinks.mk

endif

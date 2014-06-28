include new-rules/all-rules.mk

# Do not build with "NO_CSHARP"
ifeq ($(NO_CSHARP),)

include csfe/progs.mk

# Unzip the Windows 8 SDK assemblies to a place where our testsuite can find it

WIN8_DEPENDENCY_FILE:=$(CSHARP_WIN8_SDK_DIR)/mscorlib.dll

$(WIN8_DEPENDENCY_FILE): $(top_srcdir)/$(PACKAGES_DIR)/Windows8SDK.tgz
	rm -rf $(CSHARP_WIN8_SDK_DIR)
	mkdir -p $(CSHARP_WIN8_SDK_DIR)
	cd $(TEST_BIN_DIR); tar xzvf $<
	touch $@

$(COV_EMIT_CSHARP): $(WIN8_DEPENDENCY_FILE)

endif # NO_CSHARP

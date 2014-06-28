# 

# This file defines macros used by both the build system and tests.
# Note that it depends on macros already defined in paths.mk and test.mk.
# There are some tests below to verify this.
#
# Note that when this is included from paths.mk, those macros are relative
# to the root of the repository, but they are absolute when included from
# test.mk.

ifeq (,$(PATHS_COMMON_MK_INCLUDED))
PATHS_COMMON_MK_INCLUDED:=1

# First, check that some vars have already been defined in other makefiles
ifeq (,$(TEST_BIN_DIR))
$(error TEST_BIN_DIR is not already defined)
endif



ifeq ($(NO_CSHARP),)

# Tests that need the Windows 8 assemblies can reference them here
# This path comes from packages/Windows8SDK.tgz
CSHARP_WIN8_SDK_DIR:=$(TEST_BIN_DIR)/Windows8SDK

endif # !NO_CSHARP


endif # PATHS_COMMON_MK_INCLUDED

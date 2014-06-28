# 

# all-rules.mk
#
# Defines all of the target-independent configuration: paths, flags,
# and functions.
#
# Since inclusion dependencies are hard to express in Makefiles, this
# single file is the one that should always be included to get to any
# subpart.

ifeq (,$(ALL_RULES_INCLUDES))
ALL_RULES_INCLUDES:=1

# Set as "immediate"
ALL_TARGETS:=

# Remove (some of) make's default rules
.SUFFIXES:

.PHONY: default all

# pull in paths.mk that is in the same directory as this file
include $(dir $(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST)))paths.mk

# User-specific settings; these need to be read before flags, etc.,
# so the user settings can influence the flags.  In particular, I
# currently need to be able to set HAS_CCACHE to 0 for fast desktop
# since I can't seem to make the prefix compiler config work.
#
# Global to all repos for the user.
build_help test_help::
	@if !(test -f $(HOME)/prevent-personal.mk); then echo use $(HOME)/prevent-personal.mk; echo "  for per-user customization"; fi

-include $(HOME)/prevent-personal.mk

# Repository-specific.
build_help test_help::
	@if !(test -f $(top_srcdir)/personal.mk); then echo use $(top_srcdir)/personal.mk; echo "  for per-repo customization"; fi

-include $(top_srcdir)/personal.mk

# function-like includes
include $(RULES)/makefiles.mk

# compile (etc.) flags
include $(RULES)/default-flags.mk

# general-purpose functions
include $(RULES)/functions.mk

# Needs to come after "personal.mk"
include $(RULES)/pop.mk

# Act on any user-specified debug source files if we are *not*
# trying to pull in test rules.
ifeq (,$(NEW_TESTSUITE_INCLUDED))
$(foreach file,$(DEBUG_SOURCE_FILES),$(eval $(call set_debug_src_file,$(file))))
endif

endif

# 

# An include to build a single shared library, based on a shlib.mk passed to the command line

default: all

include new-rules/all-rules.mk

# You must pass SHLIB=<dir containing shlib.mk> to build a specific shared
#  library
# The path must be relative to the root
# e.g.
# $(MAKE) -C ../.. -f new-rules/make-one-shlib.mk SHLIB=lib/myshlib
ifneq (,$(SHLIB))

SOURCE_DIR:=$(SHLIB)

include $(SHLIB)/shlib.mk

EXTRA_CXXFLAGS+=$(MAKE_ONE_SHLIB_EXTRA_CXXFLAGS)
EXTRA_CFLAGS+=$(MAKE_ONE_SHLIB_EXTRA_CFLAGS)

ifneq (,$(SOURCES_OVERRIDE))
SOURCES:=$(SOURCES_OVERRIDE)
endif

ifeq (,$(NO_LINK))
include $(CREATE_SHLIB)

clean: extra-clean
	rm -f $(OBJS) $(DEPS) $(SHLIB_BINARY)

else
EXTRA_SHARED_CFLAGS+=$(PLATFORM_SHARED_PIC_CFLAGS)
include $(BUILD_SOURCES)

clean: extra-clean
	rm -f $(OBJS)

endif

.PHONY: extra-clean

else
$(error Must give a value to the SHLIB variable)

endif

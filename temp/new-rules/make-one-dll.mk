# 

# An include to build a single program, based on a prog.mk passed to the command line

default: all

include new-rules/all-rules.mk

# You must pass DLL=<dir containing dll.mk> to build a specific program
# The path must be relative to the root
# e.g.
# $(MAKE) -C ../.. -f new-rules/make-one-dll.mk DLL=dll/cs-emit-libs
ifneq (,$(DLL))

SOURCE_DIR:=$(DLL)

include $(DLL)/dll.mk

EXTRA_CXXFLAGS+=$(MAKE_ONE_PROG_EXTRA_CXXFLAGS)
EXTRA_CFLAGS+=$(MAKE_ONE_PROG_EXTRA_CFLAGS)

ifneq (,$(SOURCES_OVERRIDE))
SOURCES:=$(SOURCES_OVERRIDE)
endif

ifeq (,$(NO_LINK))
include $(CREATE_DLL)

clean: extra-clean
	rm -f $(OBJS) $(DEPS) $(PROG_BINARY)

else
include $(BUILD_SOURCES)

clean: extra-clean
	rm -f $(OBJS)

endif

.PHONY: extra-clean

else
$(error Must give a value to the DLL variable)

endif


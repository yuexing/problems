# 

# An include to build a single library, based on a directory containing a lib.mk and sources.mk passed to the command line

default: all

include new-rules/all-rules.mk

# You must pass LIB=<dir containing lib.mk> to build a specific program
# The path must be relative to the root
# e.g.
# $(MAKE) -C $(top_scrdir) -f new-rules/make-one-lib.mk LIB=libs/text
ifneq (,$(LIB))

SOURCE_DIR:=$(LIB)

include $(LIB)/lib.mk

ifneq (,$(SOURCES_OVERRIDE))
SOURCES:=$(SOURCES_OVERRIDE)
GEN_SOURCES:=
endif

ifeq (,$(NO_LINK))
ifeq ($(LIB_IS_SHARED),1)
include $(CREATE_SHLIB)
else
include $(CREATE_LIB)
endif

clean:
	rm -f $(OBJS) $(DEPS) $(LIB_BINARY)

else
include $(BUILD_SOURCES)

clean:
	rm -f $(OBJS)

endif

else

$(error Must give a value to the LIB variable)

endif


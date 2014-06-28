# 

# An include to build a set of sources, with no linking

default: all

include new-rules/all-rules.mk

# Must set prog as in create-one-lib, create-one-shlib, or
# create-one-prog, also set SOURCES
ifeq (,$(PROG)$(SHLIB)$(LIB))
$(error Must give a value to the PROG, SHLIB, or LIB variables)
endif

ifeq (,$(SOURCES))
$(error Must give a value to the SOURCES variable)
endif

# I think passing on the command line overrides by default, 
# but it's cleaner that way
SOURCES_OVERRIDE:=$(SOURCES)

NO_LINK:=1

ifneq (,$(PROG))
# PROG style
include new-rules/make-one-prog.mk
else
ifneq (,$(SHLIB))
# SHLIB style
include new-rules/make-one-shlib.mk
else
# LIB style
include new-rules/make-one-lib.mk
endif
endif

all: $(OBJS)

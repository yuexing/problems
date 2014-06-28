# 

# This file was adapted from create-prog.mk, and probably does a lot of unnecessary things

# File included when you want to create a binary
# Arguments:
# NAME: name of the program. The binary will be
# $(call PROG_BIN_NAME,$(NAME)) (see paths.mk)
#
# DEPENDENCIES: name of the required libraries (i.e., path from the
# root)
#
# EXTRA_DEPENDENCIES: Additional dependencies (as make targets) of
# this program's objects (e.g. packages, to get appropriate headers);
# reset to empty
#
# SOURCES: Sources specific to this binary (e.g. containing "main"),
# relative to SOURCE_DIR
#
# SOURCE_DIR: Directory where sources will be found
#
# BIN_DIR: Directory where the program will reside
# This value is optional, if not set it will default to $(RELEASE_BIN_DIR).
# This value is also reset to empty.
#
# CXX: The C++ compiler / linker
# CXXFLAGS: C++ flags (deferred)
# EXTRA_CXXFLAGS: C++ flags (immediate; reset to empty)
# LDFLAGS: Generic link flags (deferred)
# EXTRA_LDFLAGS: Additional link flags (immediate; reset to empty)
# EXTRA_OBJS: Additional objects for the binary (immediate; reset to empty)


# Hack:  dlls don't want -mconsole, but it comes from PLATFORM_LD_POSTFLAGS
# in mingw-flags.mk, so remove it
LD_POSTFLAGS:=$(subst -mconsole,,$(LD_POSTFLAGS))

ifeq (,$(QUIET))
$(info Adding dll $(NAME) to targets)
endif

ifeq (,$(NO_DEPS))

# Save off arguments (since e.g. variable "NAME" might be reused)
# Note that this only works if this file is not recursively included
# (it shouldn't be)
$(call $(save_and_clear_build_vars))

#$(warning add dependencies of $(NAME), deps=$(DEPENDENCIES))

LIB_LINK_FLAGS:=
LIB_DEPENDENCIES:=
LIB_EXTRA_LINK_FLAGS:=

# Arguments; DEPENDENCIES
include $(ADD_LIBS)

# Result:
# Binary dependencies = LIB_DEPENDENCIES
# Link flags = LIB_LINK_FLAGS LIB_EXTRA_LINK_FLAGS 

# Restore variables
$(call $(restore_build_vars))

else # NO_DEPS

NO_LINK:=1

endif

#$(warning lib dependencies of $(NAME): $(LIB_DEPENDENCIES))
#$(warning lib link flags of $(NAME): $(LIB_LINK_FLAGS))

# Actual target
ifeq (,$(BIN_DIR))
DLL_BINARY:=$(RELEASE_BIN_DIR)/$(NAME).dll
else
DLL_BINARY:=$(BIN_DIR)/$(NAME).dll
endif

# Make link flags immediate
$(DLL_BINARY): LINK_FLAGS:=$(LIB_LINK_FLAGS) $(LIB_EXTRA_LINK_FLAGS) $(EXTRA_LDFLAGS)

# Takes SOURCES, SOURCE_DIR, EXTRA_CXXFLAGS, EXTRA_LDFLAGS, NAME
include $(BUILD_SOURCES)
# Result in OBJS

$(DLL_BINARY): OBJS:=$(OBJS) $(EXTRA_OBJS)

#$(warning $(DLL_BINARY): lib deps = $(LIB_DEPENDENCIES))

ifeq (,$(NO_LINK))

ifeq ($(BUILD_TYPE),PROFILE)
  $(info Linking $(NAME) as PROFILE)
$(DLL_BINARY): LINK_FLAGS:=$(LINK_FLAGS) -pg
endif

ifeq ($(BUILD_TYPE),COVERAGE)
$(DLL_BINARY): LINK_FLAGS:=$(LINK_FLAGS) -fprofile-arcs -ftest-coverage
endif

ifeq ($(NO_RESOURCE_FILE),)
RESOURCE_FILE:=$(GLOBAL_RESOURCE_FILE)
else
RESOURCE_FILE:=
endif
NO_RESOURCE_FILE:=

$(DLL_BINARY): RESOURCE_FILE:=$(RESOURCE_FILE)

$(DLL_BINARY): $(OBJS) $(EXTRA_OBJS) $(LIB_DEPENDENCIES) $(RESOURCE_FILE)  
	@mkdir -p $(dir $@)
	$(CXX) -o $@ $(LD_PREFLAGS) $(OBJS) $(RESOURCE_FILE) $(LINK_FLAGS) $(LD_POSTFLAGS)
else
$(DLL_BINARY): $(OBJS) $(EXTRA_OBJS) $(LIB_DEPENDENCIES)
	@echo "No linking done"
endif

ALL_TARGETS+= $(DLL_BINARY)

all: $(DLL_BINARY)

# Reset flags as promised (allowing an empty default)
EXTRA_LDFLAGS:=
EXTRA_OBJS:=
BIN_DIR:=

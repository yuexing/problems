# 

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
# CSPROJ (C# only): .csproj file for msbuild, relative to
# SOURCE_DIR. Optional; defaults to SOURCE_DIR/NAME.csproj
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

# If any of the "sources" includes ".cs", then this is a C# program.
# Build it specially.
ifneq ($(filter %.cs, $(SOURCES)),)
CSHARP_PROG:=1
else
CSHARP_PROG:=
endif

ifeq ($(CSHARP_PROG)-$(NO_CSHARP),1-1)

# Nothing

else


ifeq (,$(QUIET))
$(info Adding program $(NAME) to targets)
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

# Allow suppressing the explicit '.exe' for .cgi files
ifeq ($(NO_EXE_POSTFIX),1)
LOCAL_EXE_POSTFIX:=
else
ifeq ($(CSHARP_PROG),1)
LOCAL_EXE_POSTFIX:=.exe
else
LOCAL_EXE_POSTFIX:=$(EXE_POSTFIX)
endif
endif
NO_EXE_POSTFIX:=

ifeq (,$(BIN_DIR))
ifeq ($(CSHARP_PROG),1)
BIN_DIR:=$(CSHARP_BIN_DIR)
else
BIN_DIR:=$(RELEASE_BIN_DIR)
endif
endif

# Actual target
PROG_BINARY:=$(BIN_DIR)/$(NAME)$(LOCAL_EXE_POSTFIX)

# Make link flags immediate
$(PROG_BINARY): LINK_FLAGS:=$(LIB_LINK_FLAGS) $(LIB_EXTRA_LINK_FLAGS) $(EXTRA_LDFLAGS)

# If any of the "sources" includes ".cs", then this is a C# program.
# Build it specially.
ifeq ($(CSHARP_PROG),1)

# C# program

$(eval $(call add_msbuild,$(PROG_BINARY)))

# Reset arguments to empty
CSPROJ:=
CS_PLATFORM:=

else

# Takes SOURCES, SOURCE_DIR, EXTRA_CXXFLAGS, EXTRA_LDFLAGS, NAME
include $(BUILD_SOURCES)
# Result in OBJS

$(PROG_BINARY): OBJS:=$(OBJS) $(EXTRA_OBJS)

#$(warning $(PROG_BINARY): lib deps = $(LIB_DEPENDENCIES))

ifeq (,$(NO_LINK))

ifeq ($(BUILD_TYPE),PROFILE)
  $(info Linking $(NAME) as PROFILE)
$(PROG_BINARY): LINK_FLAGS:=$(LINK_FLAGS) -pg
endif

ifeq ($(BUILD_TYPE),COVERAGE)
$(PROG_BINARY): LINK_FLAGS:=$(LINK_FLAGS) -fprofile-arcs -ftest-coverage
$(PROG_BINARY): CCACHE:=
endif

ifeq ($(NO_RESOURCE_FILE),)

RESOURCE_FILE:=$(GLOBAL_RESOURCE_FILE)

include $(RULES)/make-global-res.mk

else
RESOURCE_FILE:=
endif
NO_RESOURCE_FILE:=

$(PROG_BINARY): RESOURCE_FILE:=$(RESOURCE_FILE)

# Lock down the name so the following macro will work right
$(PROG_BINARY): NAME:=$(NAME)

# If we're building a program in the RELEASE_BIN_DIR, then this macro
# will record the options used to build it.  Note that during parallel builds,
# output for different targets can be interleaved.  However, each echo command
# is prefixed by the target so that we can stitch it all together when
# running utilities/create-lgpl-build.pl during "make release".
log_build_command = \
	@if [[ $@ == $(RELEASE_BIN_DIR)* ]]; then \
	    ( echo "$@:  LD_PREFLAGS = $(LD_PREFLAGS)" \
	      && echo "$@:  OBJS = $(OBJS)" \
	      && echo "$@:  RESOURCE_FILE = $(RESOURCE_FILE)" \
	      && echo "$@:  LINK_FLAGS = $(LINK_FLAGS)" \
	      && echo "$@:  LD_POSTFLAGS = $(LD_POSTFLAGS)" ) \
	    > $(LINK_COMMAND_LOG_PREFIX)_$(NAME); \
	fi

$(PROG_BINARY): $(OBJS) $(EXTRA_OBJS) $(LIB_DEPENDENCIES) $(RESOURCE_FILE)
	$(log_build_command)
	@mkdir -p $(dir $@)
	$(CXX) -o $@ $(LD_PREFLAGS) $(OBJS) $(RESOURCE_FILE) $(LINK_FLAGS) $(LD_POSTFLAGS)
else
$(PROG_BINARY): $(OBJS) $(EXTRA_OBJS) $(LIB_DEPENDENCIES)
	@echo "No linking done"
endif

endif # C# program

ALL_TARGETS+= $(PROG_BINARY)

all: $(PROG_BINARY)

# Reset flags as promised (allowing an empty default)
EXTRA_LDFLAGS:=
EXTRA_OBJS:=
BIN_DIR:=

endif # C# program + NO_CSHARP

CSHARP_PROG:=

# 

# File included when you want to create a shared library
# Can be included several times for the same shared library, will only add the file once
#
# Arguments:
# NAME: name of the shared library without platform prefix or postfix
#
# DEPENDENCIES: name of the required libraries (i.e., path from the
# root)
#
# EXTRA_DEPENDENCIES: Additional dependencies (as make targets) of
# this program's objects (e.g. packages, to get appropriate headers);
# reset to empty
#
# SOURCES: Sources specific to this shared library relative to SOURCE_DIR
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
# EXTRA_CFLAGS: C flags (immediate; reset to empty)
# EXTRA_SHARED_CFLAGS: Shared C/C++ flags (immediate; reset to empty)
# EXTRA_OBJS: Additional objects for the shared library (immediate; reset to empty)
# LDFLAGS: Generic link flags (deferred)
# EXTRA_LDFLAGS: Additional link flags (immediate; reset to empty)
#
# Output: SHLIB_BINARY = name and path for the shared library

# Clear LIB_IS_SHARED to ensure only this library is made shared.
LIB_IS_SHARED:=

# Allow suppressing shared library prefix
ifeq ($(NO_SHLIB_PREFIX),1)
LOCAL_SHLIB_PREFIX:=
else
LOCAL_SHLIB_PREFIX:=$(SHLIB_PREFIX)
endif
NO_SHLIB_PREFIX:=

# Allow suppressing shared library postfix
ifeq ($(NO_SHLIB_POSTFIX),1)
LOCAL_SHLIB_POSTFIX:=
else
LOCAL_SHLIB_POSTFIX:=$(SHLIB_POSTFIX)
endif
NO_SHLIB_POSTFIX:=

# Actual target
SHLIB_BINARY:=$(LIB_BIN_DIR)/$(LOCAL_SHLIB_PREFIX)$(NAME)$(LOCAL_SHLIB_POSTFIX)

# Shipping location
ifeq (,$(BIN_DIR))
SHIP_SHLIB_BINARY:=$(RELEASE_BIN_DIR)/$(LOCAL_SHLIB_PREFIX)$(NAME)$(LOCAL_SHLIB_POSTFIX)
else
SHIP_SHLIB_BINARY:=$(BIN_DIR)/$(LOCAL_SHLIB_PREFIX)$(NAME)$(LOCAL_SHLIB_POSTFIX)
endif
BIN_DIR:=


# Prevent another inclusion.
# Note that for this to work, library names must be unique
ifeq (,$($(NAME)_SH_INCLUDED))
$(NAME)_SH_INCLUDED:=1

#We need to save and clear these since we call ADD_LIBS ourselves
$(call save_and_clear_var,LIB_LINK_FLAGS)
$(call save_and_clear_var,LIB_DEPENDENCIES)
$(call save_and_clear_var,LIB_EXTRA_LINK_FLAGS)
$(call save_and_clear_var,ADD_LIB_LINK_FLAG)

NONSH_LIB_BINARY:=$(call LIB_BIN_NAME,$(NAME))

ifeq (,$(QUIET))
$(info Adding shared library $(NAME) to targets)
endif

# Create the static version
# Skip building of sources as that will be done later
EXTRA_SHARED_CFLAGS+=$(PLATFORM_SHARED_PIC_CFLAGS)
include $(CREATE_LIB)

#$(warning create shlib $(NAME))

ifeq (,$(NO_DEPS))

# Save off arguments (since e.g. variable "NAME" might be reused)
# Note that this only works if this file is not recursively included
# (it shouldn't be)
$(call $(save_and_clear_build_vars))

#$(warning add dependencies of $(NAME), deps=$(DEPENDENCIES))

$(call save_and_clear_var,OBJS)
$(call save_and_clear_var,EXTRA_OBJS)

# Arguments; DEPENDENCIES
include $(ADD_LIBS)

$(call restore_var,EXTRA_OBJS)
$(call restore_var,OBJS)

# Result:
# Shared library dependencies = LIB_DEPENDENCIES
# Link flags = LIB_LINK_FLAGS LIB_EXTRA_LINK_FLAGS

# Restore variables
$(call $(restore_build_vars))

else # NO_DEPS

NO_LINK:=1

endif

#$(warning lib dependencies of $(NAME): $(LIB_DEPENDENCIES))
#$(warning lib link flags of $(NAME): $(LIB_LINK_FLAGS))

# Make link flags immediate
$(SHLIB_BINARY): LINK_FLAGS:=$(LIB_LINK_FLAGS) $(LIB_EXTRA_LINK_FLAGS) $(EXTRA_LDFLAGS)

# This is done by including $(CREATE_LIB) above
# Uses: SOURCES, SOURCE_DIR, EXTRA_CXXFLAGS, EXTRA_LDFLAGS, NAME
#EXTRA_SHARED_CFLAGS+=$(PLATFORM_SHARED_PIC_CFLAGS)
#include $(BUILD_SOURCES)
# Result: OBJS, BUILD_OBJ_DIR

#$(warning shlib objs for $(NAME): $(OBJS))
$(SHLIB_BINARY): OBJS:=$(OBJS) $(EXTRA_OBJS)

#$(warning $(SHLIB_BINARY): lib deps = $(LIB_DEPENDENCIES))

ifeq (,$(NO_LINK))

ifeq ($(BUILD_TYPE),PROFILE)
$(info Linking shared library $(NAME) as PROFILE)
$(SHLIB_BINARY): LINK_FLAGS:=$(LINK_FLAGS) -pg
endif

ifeq ($(BUILD_TYPE),COVERAGE)
$(SHLIB_BINARY): LINK_FLAGS:=$(LINK_FLAGS) -fprofile-arcs -ftest-coverage
endif

ifeq ($(NO_RESOURCE_FILE),)
RESOURCE_FILE:=$(GLOBAL_RESOURCE_FILE)
else
RESOURCE_FILE:=
endif
NO_RESOURCE_FILE:=

$(SHLIB_BINARY): RESOURCE_FILE:=$(RESOURCE_FILE)

# Remove '-static' from PLATFORM_LD_*FLAGS.  Shared libraries fail to link
# if '-static' is specified.
$(SHLIB_BINARY): PLATFORM_LD_PREFLAGS:=$(filter-out -static,$(PLATFORM_LD_PREFLAGS))
$(SHLIB_BINARY): PLATFORM_LD_POSTFLAGS:=$(filter-out -static,$(PLATFORM_LD_POSTFLAGS))

$(SHLIB_BINARY): $(OBJS) $(EXTRA_OBJS) $(LIB_DEPENDENCIES) $(RESOURCE_FILE) $(NONSH_LIB_BINARY)
	@mkdir -p $(dir $@)
	@rm -f $@
	$(CXX) -o $@ $(PLATFORM_LD_SHLIBFLAGS) $(LD_PREFLAGS) $(OBJS) $(RESOURCE_FILE) $(LINK_FLAGS) $(LD_POSTFLAGS)
	cp $@ $(SHIP_SHLIB_BINARY)
else
$(SHLIB_BINARY): $(OBJS) $(EXTRA_OBJS) $(LIB_DEPENDENCIES) $(NONSH_LIB_BINARY)
	@echo "No linking done"
endif

ALL_TARGETS += $(SHLIB_BINARY)

all: $(SHLIB_BINARY)

# Reset flags as promised (allowing an empty default)
EXTRA_LDFLAGS:=
EXTRA_OBJS:=

$(call restore_var,ADD_LIB_LINK_FLAG)
$(call restore_var,LIB_EXTRA_LINK_FLAGS)
$(call restore_var,LIB_DEPENDENCIES)
$(call restore_var,LIB_LINK_FLAGS)

endif

#$(warning end shlib, shared library = $(SHLIB_BINARY))

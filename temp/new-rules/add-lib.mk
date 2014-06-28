# 

# This file adds the given library to the build, and adds its flag to the link flags
# Also adds the binary target to LIB_DEPENDENCIES
# The library won't add itself if it's already part of the link flags
#
# Arguments (typically given by lib.mk)
#
# SOURCE_DIR: Directory where the sources reside. This directory must have a sources.mk files which contains the list of sources.
#
# NAME: name of the library. Must be globally unique.
#
# DEPENDENCIES: Library dependencies (list of directories, as in ADD_LIBS)
# EXTRA_DEPENDENCIES: Additional dependencies (as make targets) of
# this library's objects (e.g. packages, to get appropriate headers);
# reset to empty
# EXTRA_ORDER_DEPENDENCIES: Same as above, but ordering only
# EXTRA_LINK_FLAGS: Additional link flags required when this library
# is included; reset to empty
# EXTRA_OBJS: Extra objects to add to the library (reset to empty)
#
# CXX: The C++ compiler (deferred)
# CXXFLAGS: C++ flags (deferred)
# EXTRA_CXXFLAGS: C++ flags (immediate; reset to empty)
#
# LIB_LINK_FLAGS: Current link flags for libraries (will be updated)
# LIB_EXTRA_LINK_FLAGS: Current link flags for libraries (will be updated)

ADD_LIB_LINK_FLAG:=-l$(NAME)

#$(warning adding $(NAME), deps = $(DEPENDENCIES))

# If the library is already in the link flags, skip
ifeq (,$(filter $(ADD_LIB_LINK_FLAG),$(LIB_LINK_FLAGS)))

#$(warning new lib deps: $(LIB_DEPENDENCIES))

#$(warning New link flags: $(LIB_LINK_FLAGS))

#$(warning pushing $(NAME))

# Push variables
$(call save_and_clear_var,LIB_IS_SHARED)
$(call save_and_clear_var,ADD_LIB_LINK_FLAG)
$(call save_and_clear_var,EXTRA_LINK_FLAGS)
# We need to save DEPENDENCIES, because building a shared library
# requires them
# Obviously though we shouldn't clear it as ADD_LIBS requires it.
$(call save_var,DEPENDENCIES)
$(call $(save_and_clear_build_vars))

# Recursively add all dependencies
# Save the build variables state before adding dependencies and restore
# the state immediately after.
include $(ADD_LIBS)

$(call $(restore_build_vars))
$(call restore_var,DEPENDENCIES)
$(call restore_var,ADD_LIB_LINK_FLAG)
$(call restore_var,EXTRA_LINK_FLAGS)
$(call restore_var,LIB_IS_SHARED)

# Build after obtaining the proper "LIB_DEPENDENCIES"
ifeq ($(LIB_IS_SHARED),1)
include $(CREATE_SHLIB)
LIB_DEPENDENCIES+= $(SHLIB_BINARY)
else
# Add library to the build
include $(CREATE_LIB)
LIB_DEPENDENCIES+= $(LIB_BINARY)
endif


# Add appropriate flags (in front) (reverse post-order)
LIB_LINK_FLAGS:=$(ADD_LIB_LINK_FLAG) $(LIB_LINK_FLAGS)

# Add EXTRA_LINK_FLAGS to LIB_EXTRA_LINK_FLAGS if they're not present already (in front)
LIB_EXTRA_LINK_FLAGS+=$(EXTRA_LINK_FLAGS)


else
#$(warning $(NAME) already in flags $(LIB_LINK_FLAGS))
endif

# Reset as promised
EXTRA_LINK_FLAGS:=

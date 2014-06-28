# 

# Add a target for the named library
# Can be included several times for the same library, will only add the file once
#
# Arguments:
# NAME: name of the library. The binary will be
# $(call LIB_BIN_NAME,$(NAME)) (see paths.mk)
# THE NAME MUST BE GLOBALLY UNIQUE. For a library, it's probably not too big a deal.
#
# SOURCES: Sources specific to this binary (e.g. containing "main"),
# relative to SOURCE_DIR
#
# SOURCE_DIR: Directory where sources will be found
#
# CXX: The C++ compiler / linker
# CXXFLAGS: C++ flags (deferred)
# EXTRA_CXXFLAGS: C++ flags (immediate; reset to empty)
# EXTRA_CFLAGS: C flags (immediate; reset to empty)
# EXTRA_SHARED_CFLAGS: Shared C/C++ flags (immediate; reset to empty)
# EXTRA_DEPENDENCIES: Additional dependencies (as make targets) of
# this library's objects (e.g. packages, to get appropriate headers);
# reset to empty
# EXTRA_OBJS: Extra objects to add to the library (reset to empty)
#
# Output: LIB_BINARY = binary for the library

# Do that even if the library target has already been created

#$(warning create lib $(NAME))

# Prevent another inclusion.
# Note that for this to work, library names must be unique
ifeq (,$($(NAME)_INCLUDED))

ifeq (,$(QUIET))
$(info Adding library $(NAME) to targets)
endif

$(NAME)_INCLUDED:=1

# List of sources in "SOURCES", relative to SOURCE_DIR
include $(SOURCE_DIR)/sources.mk

# If any of the "sources" includes ".cs", then this is a C# library.
# Build it specially.
ifneq ($(filter %.cs, $(SOURCES)),)
CSHARP_LIB:=1
# Indicates whether the library is a C# library.
# We need to check this variable every time the library is referenced, so
# save it in a globally unique variable.
# If true, then the library name is different, and the link line should not
# be affected.
$(NAME)_IS_CSHARP:=1
else
CSHARP_LIB:=
endif

ifeq ($(CSHARP_LIB)-$(NO_CSHARP),1-1)

# Nothing

else

ifeq ($(CSHARP_LIB),1)

# Very similar to what's done in create-prog.mk
# One difference though: we only build the intermediate file, because
# msbuild will copy the DLLs when building the main project anyway.

LIB_BINARY:=$(call CSHARP_LIB_BIN_NAME,$(NAME))

$(eval $(call add_msbuild,$(LIB_BINARY)))

# Reset arguments to empty
CSPROJ:=

else # C# lib

# Uses: SOURCES, SOURCE_DIR, EXTRA_CXXFLAGS
include $(BUILD_SOURCES)
# Result: OBJS, BUILD_OBJ_DIR

# Create targets for library archive members

#$(warning lib objs for $(NAME): $(OBJS))

LIB_MEMBERS:=

# This doesn't work because archive target + make -j doesn't work.
#$(foreach obj, $(OBJS), $(eval $(call add_obj_to_lib,$(obj),$(LIB_BINARY))))

#$(warning lib members for $(NAME): $(LIB_MEMBERS))

LIB_BINARY:=$(call LIB_BIN_NAME,$(NAME))
# Instead, fully recreate every time; it also avoids extra object files in libs
$(LIB_BINARY): $(OBJS) $(EXTRA_OBJS)
	@mkdir -p $(dir $@)
	@ rm -f $@
	$(CREATE_LIBRARY_ARCHIVE) $@ $^

endif # C# lib

endif # C# lib + No C#

ALL_TARGETS += $(LIB_BINARY)

all: $(LIB_BINARY)

EXTRA_OBJS:=

endif # Include guard

ifeq ($($(NAME)_IS_CSHARP),1)
LIB_BINARY:=$(call CSHARP_LIB_BIN_NAME,$(NAME))
# Prevent adding a "-l" option.
ADD_LIB_LINK_FLAG:=
else
LIB_BINARY:=$(call LIB_BIN_NAME,$(NAME))
endif

#$(warning end lib, binary = $(LIB_BINARY))

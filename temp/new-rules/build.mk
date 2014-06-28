# 

# This files creates rules to build C++ files
# Arguments:
# NAME: A unique name to identify this build
# The objects will go in a corresponding subdirectory of OBJ_DIR
# SOURCES: C/C++ source files (relative to SOURCE_DIR)
# GEN_SOURCES: Generated C/C++ source files (relative to GENSRC_DIR,
# reset to empty)
# SOURCE_DIR: Base directory for sources
# CXX: The C++ compiler (deferred)
# CC: The C++ compiler (deferred)
# CXXFLAGS: C++ flags (deferred)
# CFLAGS: C flags (deferred)
# SHARED_CFLAGS: Shared C / C++ flags (deferred)
# EXTRA_CXXFLAGS: C++ flags (immediate; reset to empty)
# EXTRA_CFLAGS: C flags (immediate; reset to empty)
# EXTRA_SHARED_CFLAGS: Shared C / C++ flags (immediate; reset to empty)
# EXTRA_DEPENDENCIES: Additional dependencies (as make targets) of
# the objects being build (e.g. packages, to get appropriate headers);
# reset to empty
# EXTRA_ORDER_DEPENDENCIES: Same as above, but ordering only
# LOCAL_BUILD_TYPE: Specific way of building the sources (e.g. DEBUG, PROFILE)
#
# Output: Objects in variable OBJS, object dir in variable BUILD_OBJ_DIR
# C objects in C_OBJS, C++ objects in CXX_OBJS
# C dependency files in C_DEPS, C++ in CXX_DEPS, all in DEPS
# C sources in C_SOURCES
# C objects in C_OBJS
# C++ sources in CXX_SOURCES
# C++ objecs in CXX_OBJS

# most of the contents of this file are meant to be evaluated repeatedly, but
# any actual targets (right now that's just build_help) are once-only, so this
# allows us to control that by only looking at them on the first time through
ifeq (,$(REPEAT_OF_BUILD_MK))
FIRST_TIME_THRU_BUILD_MK:=1
REPEAT_OF_BUILD_MK:=1
else
FIRST_TIME_THRU_BUILD_MK:=
endif


# Try not to overwrite variables
BUILD_OBJ_DIR:=$(OBJ_DIR)/objs/$(NAME)

CXX_SOURCES:=$(filter %.cpp,$(SOURCES))
CXX2_SOURCES:=$(filter %.cxx,$(SOURCES))
CC_SOURCES:=$(filter %.cc,$(SOURCES))
C_SOURCES:=$(filter %.c,$(SOURCES))
BISON_SOURCES:=$(filter %.y,$(SOURCES))
FLEX_SOURCES:=$(filter %.lex,$(SOURCES))
# Allow ".ast" files in the list of sources.
# They will be considered to be inputs to "astgen" and:
# - the first file will be used to name the output.
# - the output will include 2 headers (1 fwd, one with class definitions)
# in the same directory as the source; these headers must be checked in
# to source control; and a C++ source file in gensrc, in a subdirectory
# named after the current program/library
#
# For instance, if you have:
# SOURCES:=foo.ast foo.cpp bar.ast
#
# foo.ast and bar.ast will be combined to generate
# foo.ast.hpp and foo.ast-fwd.hpp, and a foo.ast.cpp in a subdirectory
# of $(GENSRC_DIR)
#
# There's currently no support for multiple sets of .ast files in a
# single library or program, atlhough you can call add_ast_files manually.
AST_SOURCES:=$(filter %.ast,$(SOURCES))

ifneq ($(AST_SOURCES),)

# This modifies $(GEN_SOURCES), so do it before handling this variable
$(eval $(call add_ast_files,$(foreach ast_file,$(AST_SOURCES),$(SOURCE_DIR)/$(ast_file)),$(NAME)))

endif

$(foreach bison_source,$(BISON_SOURCES),$(eval $(call add_bison_file,$(SOURCE_DIR)/$(bison_source),$(NAME))))
$(foreach flex_source,$(FLEX_SOURCES),$(eval $(call add_flex_file,$(SOURCE_DIR)/$(flex_source),$(NAME))))

# Different variables for generated source: generated source is not in
# SOURCE_DIR, but in GENSRC_DIR
CXX_GEN_SOURCES:=$(filter %.cpp,$(GEN_SOURCES))
CXX2_GEN_SOURCES:=$(filter %.cxx,$(GEN_SOURCES))
CC_GEN_SOURCES:=$(filter %.cc,$(GEN_SOURCES))
C_GEN_SOURCES:=$(filter %.c,$(GEN_SOURCES))

GARBAGE:=$(filter-out %.cpp %.cxx %.c %.cc %.ast %.lex %.y,$(SOURCES))

ifneq (,$(GARBAGE))
$(error Don\'t know what to do with unknown source: $(GARBAGE))
endif

# Include program or library-specific "personal settings" (e.g. LOCAL_BUILD_TYPE)
-include $(SOURCE_DIR)/personal.mk

ifeq ($(BUILD_FE_SUBSET),1)
EXTRA_SHARED_CFLAGS += -D__MC_BUILD_OFFSHORE_SUBSET
endif

ifeq (,$(LOCAL_BUILD_TYPE))
LOCAL_BUILD_TYPE:=$(BUILD_TYPE)
endif

ifneq (,$(FIRST_TIME_THRU_BUILD_MK))
build_help::
	@echo "BUILD_TYPE={DEBUG,DEBUG2,PROFILE,COVERAGE}"
endif

ifeq ($(LOCAL_BUILD_TYPE),DEBUG)
  # $(info) is the easiest way to print a message
  # This is useful because it's easy to have a typo in BUILD_TYPE and
  # it's practically impossible to detect
  $(info Building $(NAME) as DEBUG)
  EXTRA_SHARED_CFLAGS+= -g
else ifeq ($(LOCAL_BUILD_TYPE),PROFILE)
  $(info Building $(NAME) as PROFILE)
  EXTRA_SHARED_CFLAGS+= $(PLATFORM_SHARED_OPTIMIZER_CFLAGS) -pg
else ifeq ($(LOCAL_BUILD_TYPE),DEBUG2)
  $(info Building $(NAME) as DEBUG2)
  EXTRA_SHARED_CFLAGS+= $(PLATFORM_SHARED_OPTIMIZER_CFLAGS) -g
else ifeq ($(LOCAL_BUILD_TYPE),COVERAGE)
  $(info Building $(NAME) as COVERAGE)
  # Do not optimize coverage builds; that can confuse gcov.
  EXTRA_SHARED_CFLAGS+= -fprofile-arcs -ftest-coverage
else ifeq ($(LOCAL_BUILD_TYPE),NORMAL)
  # SGM: I need a non-empty name for the "normal" build so I can
  # force it from capture.mk.
  EXTRA_SHARED_CFLAGS+= $(PLATFORM_SHARED_OPTIMIZER_CFLAGS)
else ifeq ($(LOCAL_BUILD_TYPE),)
  EXTRA_SHARED_CFLAGS+= $(PLATFORM_SHARED_OPTIMIZER_CFLAGS)
else
  $(error Invalid BUILD_TYPE for $(NAME): $(BUILD_TYPE))
endif

ifeq ($(findstring packages/,$(SOURCE_DIR)),packages/)
# Disable all warnings in the source packages
EXTRA_SHARED_CFLAGS+= -w -Wno-error
else
EXTRA_SHARED_CFLAGS+= $(PLATFORM_WARNING_FLAGS)
endif

C_OBJS:=
C_DEPS:=
$(foreach c_source,$(C_SOURCES),$(eval $(call add_c_file,$(SOURCE_DIR)/$(c_source))))
$(foreach c_source,$(C_GEN_SOURCES),$(eval $(call add_c_file,$(GENSRC_DIR)/$(c_source))))

CXX_OBJS:=
CXX_DEPS:=
$(foreach cxx_source,$(CXX_SOURCES),$(eval $(call add_cxx_file,$(SOURCE_DIR)/$(cxx_source))))
$(foreach cxx_source,$(CXX2_SOURCES),$(eval $(call add_cxx2_file,$(SOURCE_DIR)/$(cxx_source))))
$(foreach cc_source,$(CC_SOURCES),$(eval $(call add_cc_file,$(SOURCE_DIR)/$(cc_source))))
$(foreach cxx_source,$(CXX_GEN_SOURCES),$(eval $(call add_cxx_file,$(GENSRC_DIR)/$(cxx_source))))
$(foreach cxx_source,$(CXX2_GEN_SOURCES),$(eval $(call add_cxx2_file,$(GENSRC_DIR)/$(cxx_source))))
$(foreach cc_source,$(CC_GEN_SOURCES),$(eval $(call add_cc_file,$(GENSRC_DIR)/$(cc_source))))

OBJS:= $(CXX_OBJS) $(C_OBJS)
DEPS:=$(C_DEPS) $(CXX_DEPS)

# Reset EXTRA_CXXFLAGS, as promised
EXTRA_CXXFLAGS:=
# Reset EXTRA_CFLAGS, as promised
EXTRA_CFLAGS:=
# Reset EXTRA_SHARED_CFLAGS, as promised
EXTRA_SHARED_CFLAGS:=
PRE_EXTRA_SHARED_CFLAGS:=
EXTRA_DEPENDENCIES:=
EXTRA_ORDER_DEPENDENCIES:=
LOCAL_BUILD_TYPE:=
GEN_SOURCES:=

